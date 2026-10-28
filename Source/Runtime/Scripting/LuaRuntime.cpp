#include "EnginePCH.h"
#include "Scripting/LuaRuntime.h"
#include "Scripting/LuaBindings.h"
#include "GameFramework/Actor.h"
#include "Component/ScriptComponent.h"
#include "Containers/Array.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include <algorithm>
#include <cmath>
#include "Engine/Engine.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/WaveCameraShake.h"
#include "Camera/PerlinCameraShake.h"
#include "Camera/CurveCameraShake.h"
#include <cassert>
#include "Camera/CameraTypes.h"
#include "Camera/CameraTransitionCurve.h"

// 기본 라이브러리 준비 후 공통 C++ 바인딩
FLuaRuntime::FLuaRuntime()
{
	Lua.open_libraries(
		sol::lib::base,
		sol::lib::math,
		sol::lib::table,
		sol::lib::string,
		sol::lib::coroutine,
		sol::lib::package);

	// 프로젝트 모듈 경로 우선 검색
	sol::table Package = Lua["package"];
	const FString ExistingPath = Package["path"];
	Package["path"] = FString("Assets/Scripts/?.lua;Assets/Scripts/?/init.lua;") + ExistingPath;

	LuaBindings::Register(Lua);

	// 대기 시간 반환 후 실행 양보
	// wait 호출 시, coroutine.yield(Seconds) 호출됨.
	sol::protected_function_result Result = Lua.safe_script(R"(
			function wait(Seconds)
			return coroutine.yield(Seconds)
			end
)", sol::script_pass_on_error);

	CheckResult(Result, "<runtime>", "RegisterWait");

}

// sol::state가 소멸하면서 VM을 해제합니다.
FLuaRuntime::~FLuaRuntime() = default;

// 일반 전역 변수와 _G 쓰기를 각 환경에 보관합니다.
sol::environment FLuaRuntime::CreateEnvironment(AActor* OwnerActor)
{
	sol::environment Environment(Lua, sol::create, Lua.globals());
	Environment["_G"] = Environment;
	Environment["obj"] = TWeakObjectPtr<AActor>(OwnerActor);

	// 환경에 연결할 World의 비소유 참조
	const TWeakObjectPtr<UWorld> WeakWorld(OwnerActor ? OwnerActor->GetWorld() : nullptr);

	// 환경별 World API
	sol::table WorldTable = Lua.create_table();
	WorldTable.set_function("find_actor",
		[WeakWorld](sol::this_state State, const FString& Name) -> sol::object
		{
			UWorld* World = WeakWorld.Get();
			ULevel* Level = World ? World->GetPersistentLevel() : nullptr;
			if (!Level)
			{
				return sol::make_object(State, sol::nil);
			}

			// 기존 FName 정책에 따른 배치 Actor 검색
			const FName SearchName(Name);
			for (AActor* Actor : Level->GetActors())
			{
				if (Actor && !Actor->IsPendingDestroy()	&& Actor->GetFName() == SearchName)
				{
					return sol::make_object(State, TWeakObjectPtr<AActor>(Actor));
				}
			}
			return sol::make_object(State, sol::nil);
		});
	// 카메라를 가진 Actor로 게임 시점 변경
	WorldTable.set_function("set_view_target",
		[WeakWorld](sol::object Target,sol::variadic_args Args) -> bool
		{
			UWorld* World = WeakWorld.Get();
			if (!World || !World->AcceptsChanges() || Args.size() > 4)
			{
				return false;
			}

			FViewTargetTransitionParams Params;
			FString FunctionName = "Cubic";
			const auto ReadArg = [&Args]<typename T>(std::size_t Index, sol::type Expected, T & Value) -> bool
			{
				if (Index >= Args.size() || Args.get_type(Index) == sol::type::nil)
				{
					return true;
				}

				if (Args.get_type(Index) != Expected)
				{
					return false;
				}

				Value = Args.get<T>(Index);
				return true;
			};

			// 위치별 인수 해석과 수치 범위 확인
			if (!ReadArg(0, sol::type::number, Params.BlendTime)
				|| !ReadArg(1, sol::type::boolean, Params.bLockOutgoing)
				|| !ReadArg(2, sol::type::string, FunctionName)
				|| !ReadArg(3, sol::type::number, Params.BlendExp)
				|| !std::isfinite(Params.BlendTime) || Params.BlendTime < 0.0f
				|| !std::isfinite(Params.BlendExp) || Params.BlendExp <= 0.0f)
			{
				return false;
			}

			// 지원하는 보간 이름의 명시적 변환
			if (FunctionName == "Linear")
			{
				Params.BlendFunction = EViewTargetBlendFunction::Linear;
			}
			else if (FunctionName == "Cubic")
			{
				Params.BlendFunction = EViewTargetBlendFunction::Cubic;
			}
			else if (FunctionName == "EaseIn")
			{
				Params.BlendFunction = EViewTargetBlendFunction::EaseIn;
			}
			else if (FunctionName == "EaseOut")
			{
				Params.BlendFunction = EViewTargetBlendFunction::EaseOut;
			}
			else if (FunctionName == "EaseInOut")
			{
				Params.BlendFunction = EViewTargetBlendFunction::EaseInOut;
			}
			else
			{
				return false;
			}

			// nil 요청의 즉시 Fallback 복귀
			if (Target.get_type() == sol::type::nil)
			{
				return World->SetViewTarget(nullptr);
			}

			if (!Target.is<TWeakObjectPtr<AActor>>())
			{
				return false;
			}

			// 소멸한 참조의 Fallback 전환 방지
			AActor* TargetActor = Target.as<TWeakObjectPtr<AActor>>().Get();
			return TargetActor && World->SetViewTarget(TargetActor, Params);

		});
	// 다음 프레임 씬 전환 예약
	WorldTable.set_function("change_scene",
		[WeakWorld](const FString& ScenePath) -> bool
		{
			UWorld* World = WeakWorld.Get();
			return GEngine && World && World->AcceptsChanges()
				&& !ScenePath.empty()
				&& GEngine->RequestSceneChange(World, ScenePath);
		});

	// Standalone 게임 종료 요청
	WorldTable.set_function("quit",
		[WeakWorld]() -> bool
		{
			return GEngine && GEngine->RequestExit(WeakWorld.Get());
		});
	Environment["world"] = WorldTable;
	return Environment;
}

// 기본 오류 핸들러의 예외 정책에 의존하지 않고 실행 결과를 받습니다.
bool FLuaRuntime::LoadScript(
	const FString& ScriptPath, sol::environment& Environment)
{
	sol::protected_function_result Result = Lua.safe_script_file(
		ScriptPath, Environment,
		sol::script_pass_on_error, sol::load_mode::text);

	return CheckResult(Result, ScriptPath, "Load");
}

// 현재 coroutine 함수가 아닌 틱그룹 변경도 연결중임..
void FLuaRuntime::BindCoroutineFunctions(sol::environment& Environment, UScriptComponent* Owner, const FString& ScriptPath)
{
	// 소유 컴포넌트의 비소유 참조
	const TWeakObjectPtr<UScriptComponent> WeakOwner(Owner);

	Environment.set_function("start_coroutine",
		[this, WeakOwner, ScriptPath](sol::protected_function Function) -> uint32
		{
			UScriptComponent* OwnerComponent = WeakOwner.Get();
			if (OwnerComponent && OwnerComponent->IsLoadingScript())
			{
				throw sol::error("start_coroutine is unavailable while loading script");
			}

			return CreateCoroutine(OwnerComponent, Function, ScriptPath);
		});

	// 환경별 컴포넌트 설정 함수
	sol::table Script = Lua.create_table();
	Script.set_function("set_tick_group",
		[WeakOwner](const FString& GroupName)
		{
			UScriptComponent* OwnerComponent = WeakOwner.Get();
			if (!OwnerComponent)
			{
				throw sol::error("Script component is unavailable");
			}

			ETickingGroup Group = ETickingGroup::Max;
			if (GroupName == "PrePhysics")
			{
				Group = ETickingGroup::PrePhysics;
			}
			else if (GroupName == "DuringPhysics")
			{
				Group = ETickingGroup::DuringPhysics;
			}
			else if (GroupName == "PostPhysics")
			{
				Group = ETickingGroup::PostPhysics;
			}
			else if (GroupName == "PostUpdateWork")
			{
				Group = ETickingGroup::PostUpdateWork;
			}
			else
			{
				throw sol::error("Unsupported tick group: " + GroupName);
			}

			OwnerComponent->RequestInitialTickGroup(Group);
		});

	Environment["script"] = Script;
}

void FLuaRuntime::BindCameraFunctions(sol::environment& Environment, UScriptComponent* Owner)
{
	// 기존 환경의 World API 확장
	sol::table WorldTable = Environment["world"];
	const TWeakObjectPtr<UScriptComponent> WeakOwner(Owner);

	WorldTable.set_function("start_camera_shake",
		[WeakOwner](const FString& Name, const sol::table& Settings,
			sol::optional<sol::object> RequestedScale) -> bool
		{
			UScriptComponent* Script = WeakOwner.Get();
			if (Script && Script->IsLoadingScript())
			{
				throw sol::error("Camera shake is unavailable while loading script");
			}
			if (!Script || !Script->CanRunGameplay())
			{
				return false;
			}

			// 설정 오류는 실제 재생 변경 전에 처리
			const FName PresetName(Name);
			if (PresetName.IsNone())
			{
				throw sol::error("Camera shake name is required");
			}

			FString PatternName = "Wave";
			sol::object PatternValue = Settings.raw_get<sol::object>("pattern");

			if (PatternValue.valid() && PatternValue.get_type() != sol::type::nil)
			{
				if (PatternValue.get_type() != sol::type::string)
				{
					throw sol::error("Camera shake pattern must be a string");
				}
				PatternName = PatternValue.as<FString>();
			}

			float Scale = 1.0f;
			if (RequestedScale && RequestedScale->get_type() != sol::type::nil)
			{
				if (!RequestedScale->is<float>())
				{
					throw sol::error("Camera shake scale must be numeric");
				}
				Scale = RequestedScale->as<float>();
			}
			if (!std::isfinite(Scale))
			{
				throw sol::error("Camera shake scale must be finite");
			}

			// 숫자 필드의 기본값·타입·유한성 검증
			const auto ReadNumber = 
				[](const sol::table& Table, const char* Key, float DefaultValue, bool bRequired = false) -> float
				{
					sol::object Value = Table.raw_get<sol::object>(Key);
					if (!Value.valid() || Value.get_type() == sol::type::nil)
					{
						if (bRequired)
						{
							throw sol::error(FString("Missing shake field: ") + Key);
						}
						return DefaultValue;
					}
					if (!Value.is<float>())
					{
						throw sol::error(FString("Invalid shake number: ") + Key);
					}

					const float Number = Value.as<float>();
					if (!std::isfinite(Number))
					{
						throw sol::error(FString("Non-finite shake number: ") + Key);
					}
					return Number;
				};

			// 생략한 축은 기존 파형 기본값 사용
			const auto ReadWave = [&ReadNumber](const sol::table& Table, const char* Key) -> FWaveOscillator
				{
					FWaveOscillator Wave;
					sol::object Value = Table.raw_get<sol::object>(Key);
					if (!Value.valid() || Value.get_type() == sol::type::nil)
					{
						return Wave;
					}
					if (!Value.is<sol::table>())
					{
						throw sol::error(FString("Invalid shake axis: ") + Key);
					}

					const sol::table Axis = Value.as<sol::table>();
					Wave.Amplitude = ReadNumber(Axis, "amplitude", Wave.Amplitude);
					Wave.Frequency = ReadNumber(Axis, "frequency", Wave.Frequency);
					Wave.Phase = ReadNumber(Axis, "phase", Wave.Phase);
					return Wave;
				};
			// 축별 기본 Seed와 전체 uint32 범위 보존
			const auto ReadPerlin = [&ReadNumber](const sol::table& Table, const char* Key,
				uint32 DefaultSeed) -> FPerlinNoiseShaker
				{
					FPerlinNoiseShaker Noise;
					Noise.Seed = DefaultSeed;

					sol::object Value = Table.raw_get<sol::object>(Key);
					if (!Value.valid() || Value.get_type() == sol::type::nil)
					{
						return Noise;
					}
					if (!Value.is<sol::table>())
					{
						throw sol::error(FString("Invalid shake axis: ") + Key);
					}

					const sol::table Axis = Value.as<sol::table>();
					Noise.Amplitude = ReadNumber(Axis, "amplitude", Noise.Amplitude);
					Noise.Frequency = ReadNumber(Axis, "frequency", Noise.Frequency);

					sol::object SeedValue = Axis.raw_get<sol::object>("seed");
					if (SeedValue.valid() && SeedValue.get_type() != sol::type::nil)
					{
						if (SeedValue.get_type() != sol::type::number)
						{
							throw sol::error(FString("Invalid Perlin seed: ") + Key);
						}

						const double Seed = SeedValue.as<double>();
						if (!std::isfinite(Seed) || Seed < 0.0 || Seed > 4294967295.0 || std::floor(Seed) != Seed)
						{
							throw sol::error(FString("Perlin seed must be a uint32 integer: ") + Key);
						}
						Noise.Seed = static_cast<uint32>(Seed);
					}

					return Noise;
				};
			// 숫자 시간과 변화량의 매핑을 기존 곡선으로 변환
			const auto ReadCurve = [](const sol::table& Table, const char* Key) -> FRichCurve
				{
					FRichCurve Curve;

					sol::object Value = Table.raw_get<sol::object>(Key);
					if (!Value.valid() || Value.get_type() == sol::type::nil)
					{
						return Curve;
					}
					if (!Value.is<sol::table>())
					{
						throw sol::error(FString("Invalid shake curve: ") + Key);
					}

					const sol::table Axis = Value.as<sol::table>();
					for (const auto& Pair : Axis)
					{
						if (Pair.first.get_type() != sol::type::number || Pair.second.get_type() != sol::type::number)
						{
							throw sol::error(FString("Curve time and value must be numeric: ") + Key);
						}

						const float Time = Pair.first.as<float>();
						const float Number = Pair.second.as<float>();
						if (!std::isfinite(Time) || Time < 0.0f || !std::isfinite(Number))
						{
							throw sol::error(FString("Invalid curve time or value: ") + Key);
						}

						Curve.AddKey(Time, Number);
					}

					return Curve;
				};
			const float Duration = ReadNumber(Settings, "duration", 0.0f, true);
			const float BlendIn = ReadNumber(Settings, "blend_in", 0.0f, true);
			const float BlendOut = ReadNumber(Settings, "blend_out", 0.0f, true);
			bool bSingleInstance = false;
			sol::object Single = Settings.raw_get<sol::object>("single_instance");
			if (Single.valid() && Single.get_type() != sol::type::nil)
			{
				if (!Single.is<bool>())
				{
					throw sol::error("single_instance must be boolean");
				}
				bSingleInstance = Single.as<bool>();
			}

			ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal;
			sol::object SpaceValue = Settings.raw_get<sol::object>("play_space");
			if (SpaceValue.valid() && SpaceValue.get_type() != sol::type::nil)
			{
				if (!SpaceValue.is<FString>())
				{
					throw sol::error("Camera shake play_space must be a string");
				}

				const FString Space = SpaceValue.as<FString>();
				if (Space == "World")
				{
					PlaySpace = ECameraShakePlaySpace::World;
				}
				else if (Space != "CameraLocal")
				{
					throw sol::error("Unsupported shake play_space");
				}
			}

			// 패턴별 파싱 결과만 초기화 함수에 복사
			UClass* ShakeClass = nullptr;
			FCameraShakeInitializer Initialize;

			if (PatternName == "Wave")
			{
				const FWaveOscillator X = ReadWave(Settings, "x");
				const FWaveOscillator Y = ReadWave(Settings, "y");
				const FWaveOscillator Z = ReadWave(Settings, "z");
				const FWaveOscillator Pitch = ReadWave(Settings, "pitch");
				const FWaveOscillator Yaw = ReadWave(Settings, "yaw");
				const FWaveOscillator Roll = ReadWave(Settings, "roll");
				const FWaveOscillator FOV = ReadWave(Settings, "fov");

				ShakeClass = UWaveCameraShake::StaticClass();
				Initialize = [Duration, BlendIn, BlendOut, X, Y, Z, Pitch, Yaw, Roll, FOV](UCameraShakeBase& Shake)
					{
						auto* Pattern = Cast<UWaveOscillatorCameraShakePattern>(Shake.GetRootShakePattern());
						assert(Pattern != nullptr);

						Pattern->Duration = Duration;
						Pattern->BlendInTime = BlendIn;
						Pattern->BlendOutTime = BlendOut;
						Pattern->X = X;
						Pattern->Y = Y;
						Pattern->Z = Z;
						Pattern->Pitch = Pitch;
						Pattern->Yaw = Yaw;
						Pattern->Roll = Roll;
						Pattern->FOV = FOV;
					};
			}
			else if (PatternName == "Perlin")
			{
				const FPerlinNoiseShaker X = ReadPerlin(Settings, "x", 11);
				const FPerlinNoiseShaker Y = ReadPerlin(Settings, "y", 23);
				const FPerlinNoiseShaker Z = ReadPerlin(Settings, "z", 37);
				const FPerlinNoiseShaker Pitch = ReadPerlin(Settings, "pitch", 53);
				const FPerlinNoiseShaker Yaw = ReadPerlin(Settings, "yaw", 71);
				const FPerlinNoiseShaker Roll = ReadPerlin(Settings, "roll", 89);
				const FPerlinNoiseShaker FOV = ReadPerlin(Settings, "fov", 107);

				ShakeClass = UPerlinCameraShake::StaticClass();
				Initialize = [Duration, BlendIn, BlendOut, X, Y, Z, Pitch, Yaw, Roll, FOV](UCameraShakeBase& Shake)
					{
						auto* Pattern = Cast<UPerlinNoiseCameraShakePattern>(Shake.GetRootShakePattern());
						assert(Pattern != nullptr);

						Pattern->Duration = Duration;
						Pattern->BlendInTime = BlendIn;
						Pattern->BlendOutTime = BlendOut;
						Pattern->X = X;
						Pattern->Y = Y;
						Pattern->Z = Z;
						Pattern->Pitch = Pitch;
						Pattern->Yaw = Yaw;
						Pattern->Roll = Roll;
						Pattern->FOV = FOV;
					};
			}
			else if (PatternName == "Curve")
			{
				const FRichCurve X = ReadCurve(Settings, "x");
				const FRichCurve Y = ReadCurve(Settings, "y");
				const FRichCurve Z = ReadCurve(Settings, "z");
				const FRichCurve Pitch = ReadCurve(Settings, "pitch");
				const FRichCurve Yaw = ReadCurve(Settings, "yaw");
				const FRichCurve Roll = ReadCurve(Settings, "roll");
				const FRichCurve FOV = ReadCurve(Settings, "fov");

				ShakeClass = UCurveCameraShake::StaticClass();
				Initialize = [Duration, BlendIn, BlendOut, X, Y, Z, Pitch, Yaw, Roll, FOV](UCameraShakeBase& Shake)
					{
						auto* Pattern = Cast<UCurveCameraShakePattern>(Shake.GetRootShakePattern());
						assert(Pattern != nullptr);

						Pattern->Duration = Duration;
						Pattern->BlendInTime = BlendIn;
						Pattern->BlendOutTime = BlendOut;
						Pattern->X = X;
						Pattern->Y = Y;
						Pattern->Z = Z;
						Pattern->Pitch = Pitch;
						Pattern->Yaw = Yaw;
						Pattern->Roll = Roll;
						Pattern->FOV = FOV;
					};
			}
			else
			{
				throw sol::error("Unsupported camera shake pattern");
			}

			// 검증된 설정으로 기존 Manager 재생 요청
			UWorld* World = Script->GetOwner()->GetWorld();
			APlayerCameraManager* Manager = World->GetPlayerCameraManager();

			return Manager && Manager->StartCameraShake(PresetName, ShakeClass, bSingleInstance, Initialize, Scale, PlaySpace) != nullptr;
		});
}

uint32 FLuaRuntime::CreateCoroutine(UScriptComponent* Owner, const sol::protected_function& Function, const FString& ScriptPath)
{
	// 실행 함수 검증
	if (!Function.valid())
	{
		LogError(ScriptPath, "CreateCoroutine", "Invalid Lua function");
		return 0;
	}

	// 소유 컴포넌트 검증
	if (!Owner || !Owner->CanRunGameplay() || Owner->GetOwner()->GetWorld()->GetLuaRuntime() != this)
	{
		LogError(ScriptPath, "CreateCoroutine", "Invalid coroutine owner");
		return 0;
	}

	// 별도 실행 스택과 함수 연결
	FLuaCoroutine Entry;
	Entry.Thread = sol::thread::create(Lua.lua_state());
	Entry.Function = sol::coroutine(Entry.Thread.thread_state(), Function);
	Entry.Owner = Owner;
	Entry.ScriptPath = ScriptPath;

	// 실행 상태 등록
	const uint32 CoroutineID = NextCoroutineID++;
	Coroutines.Add(CoroutineID, Entry);
	return CoroutineID;
}

sol::call_status FLuaRuntime::ResumeCoroutine(uint32 CoroutineID)
{
	// 실행 상태 조회
	FLuaCoroutine* Entry = Coroutines.Find(CoroutineID);
	if (!Entry)
	{
		LogError("<unknown>", "ResumeCoroutine", "Unknown or finished coroutine");
		return sol::call_status::runtime;
	}
	// 중첩 재개 차단
	if (CurrentCoroutineID != 0)
	{
		LogError(Entry->ScriptPath, "ResumeCoroutine", "Nested coroutine resume is not supported");
		return sol::call_status::runtime;
	}
	// 실행 불가능한 소유 작업 제거
	UScriptComponent* Owner = Entry->Owner.Get();
	if (!Owner || !Owner->HasBegunPlay() || !Owner->CanRunGameplay())
	{
		Coroutines.Remove(CoroutineID);
		return sol::call_status::runtime;
	}
	CurrentCoroutineID = CoroutineID;
	sol::call_status Status;
	{
		// 결과 정리 범위
		sol::protected_function_result Result = Entry->Function();
		Status = Result.status();
		CheckResult(Result, Entry->ScriptPath, "ResumeCoroutine");

		// yield 결과를 다음 대기 시간으로 저장
		if (Status == sol::call_status::yielded && !Entry->bCancelled)
		{
			double WaitTime = 0.0;
			if (Result.return_count() > 0)
			{
				WaitTime = Result.get_type() == sol::type::number ? Result.get<double>() : -1.0;
			}

			if (!std::isfinite(WaitTime) || WaitTime < 0.0)
			{
				LogError(Entry->ScriptPath, "ResumeCoroutine", "Yield must provide a finite, non-negative wait time");
				Status = sol::call_status::runtime;
			}
			else
			{
				Entry->RemainingWait = WaitTime;
			}
		}
	}

	CurrentCoroutineID = 0;

	// 실행 중단된 상태가 아니라면->함수 완료/실행 실패 둘중 하나인 경우, 작업 제거
	if (Entry->bCancelled || Status != sol::call_status::yielded)
	{
		Coroutines.Remove(CoroutineID);
	}

	return Status;
}

void FLuaRuntime::CancelCoroutines(UScriptComponent* Owner)
{
	// 빈 소유자 제외
	if (!Owner)
	{
		return;
	}

	// 취소 대상 수집
	TArray<uint32> CoroutineIDs;
	for (const auto& [CoroutineID, Entry] : Coroutines)
	{
		if (Entry.Owner.Get() == Owner)
		{
			CoroutineIDs.Add(CoroutineID);
		}
	}

	// 수집 후 취소
	for (uint32 CoroutineID : CoroutineIDs)
	{
		CancelCoroutine(CoroutineID);
	}
}

void FLuaRuntime::CancelAllCoroutines()
{
	// 전체 취소 대상 수집
	TArray<uint32> CoroutineIDs;
	for (const auto& [CoroutineID, Entry] : Coroutines)
	{
		CoroutineIDs.Add(CoroutineID);
	}

	// 수집 후 취소
	for (uint32 CoroutineID : CoroutineIDs)
	{
		CancelCoroutine(CoroutineID);
	}

}

// 실패 결과가 확인된 경우에만 Lua 오류로 변환합니다.
bool FLuaRuntime::CheckResult(
	const sol::protected_function_result& Result,
	const FString& ScriptPath, const char* EventName) const
{
	if (Result.valid())
		return true;

	const sol::error Error = Result;
	LogError(ScriptPath, EventName, Error.what());
	return false;
}

// 모든 실행 오류에 파일명과 호출 위치를 함께 출력합니다.
void FLuaRuntime::LogError(const FString& ScriptPath,
	const char* EventName, const char* Message) const
{
	HTR_LOG(Error, "[Lua][{}][{}] {}", ScriptPath, EventName, Message);
}

void FLuaRuntime::CancelCoroutine(uint32 CoroutineID)
{
	// 실행 상태 조회
	FLuaCoroutine* Entry = Coroutines.Find(CoroutineID);
	if (!Entry)
	{
		return;
	}

	// 실행 중 제거 지연
	if (CoroutineID == CurrentCoroutineID)
	{
		Entry->bCancelled = true;
		return;
	}

	Coroutines.Remove(CoroutineID);
}

void FLuaRuntime::PrepareCoroutineTick()
{
	// 실행 중 갱신 대상 교체 차단
	if (CurrentCoroutineID != 0)
	{
		LogError("<runtime>", "PrepareCoroutineTick", "Nested coroutine tick is not supported");
		return;
	}

	CoroutineTickIDs.Reset();
	TArray<uint32> InvalidCoroutineIDs;

	// 소유 컴포넌트별 프레임 대상 수집
	for (const auto& [CoroutineID, Entry] : Coroutines)
	{
		UScriptComponent* Owner = Entry.Owner.Get();
		if (!Owner || !Owner->CanRunGameplay())
		{
			InvalidCoroutineIDs.Add(CoroutineID);
			continue;
		}

		CoroutineTickIDs[Owner->GetUUID()].Add(CoroutineID);
	}

	// 수집 후 무효 작업 취소
	for (uint32 CoroutineID : InvalidCoroutineIDs)
	{
		CancelCoroutine(CoroutineID);
	}

	// 소유 컴포넌트 내 생성 순서 유지
	for (auto& [OwnerID, CoroutineIDs] : CoroutineTickIDs)
	{
		std::sort(CoroutineIDs.begin(), CoroutineIDs.end());
	}
}

void FLuaRuntime::TickCoroutines(UScriptComponent* Owner, float DeltaTime)
{
	// 실행 중 중첩 갱신 차단
	if (CurrentCoroutineID != 0)
	{
		LogError("<runtime>", "TickCoroutines",	"Nested coroutine tick is not supported");
		return;
	}

	if (!Owner)
	{
		return;
	}

	// 이번 프레임의 소유 작업을 한 번만 인출
	TArray<uint32> CoroutineIDs;
	if (!CoroutineTickIDs.RemoveAndCopyValue(Owner->GetUUID(), CoroutineIDs))
	{
		return;
	}

	for (uint32 CoroutineID : CoroutineIDs)
	{
		// 이전 콜백의 종료·삭제 요청 반영
		if (!Owner->HasBegunPlay() || !Owner->CanRunGameplay())
		{
			CancelCoroutines(Owner);
			break;
		}

		FLuaCoroutine* Entry = Coroutines.Find(CoroutineID);
		if (!Entry)
		{
			continue;
		}

		Entry->RemainingWait -= static_cast<double>(DeltaTime);
		if (Entry->RemainingWait <= 0.0)
		{
			ResumeCoroutine(CoroutineID);
		}
	}
}