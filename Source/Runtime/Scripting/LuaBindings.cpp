#include "EnginePCH.h"
#include "Scripting/LuaBindings.h"
#include "Math/Vector.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtr.h"
#include "Component/SceneComponent.h"
#include "Input/InputSystem.h"
#include "Math/Rotator.h"
#include "GameFramework/Character.h"
#include "Core/GameHud.h"
#include "Core/TimeDilation.h"
#include "Cinematic/SequencePlayer.h"
#include "GameFramework/RacingCar.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Swamp.h"
#include "Component/ExponentialHeightFogComponent.h"
#include "Component/ParticleSubUVComponent.h"
#include "Collision/HitResult.h"
#include "Component/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Audio/SoundManager.h"
#include <algorithm>
#include <cassert>
#include <format>
#include <fstream>
#include <iomanip>
#include <tuple>

namespace LuaBindings
{
	// Lua 키 이름->키코드 변환 함수
	// 지원하고싶은 입력이 생길때마다 변경해야함.
	static EKeyCode ResolveInputKey(const FString& Key)
	{
		if (Key == "W" || Key == "A" || Key == "S" || Key == "D" || Key == "R" || Key == "Q" || Key == "E")
		{
			return static_cast<EKeyCode>(Key[0]);
		}
		if (Key == "Ctrl")
		{
			return EKeyCode::Control;
		}

		if (Key == "Escape")
		{
			return EKeyCode::Escape;
		}
		if (Key == "Space")
		{
			return EKeyCode::Space;
		}

		throw sol::error("Unsupported input key: " + Key);
	}

	// Actor 소유 파티클 컴포넌트 조회
	static UParticleSubUVComponent* ResolveParticleComponent(const TWeakObjectPtr<AActor>& Reference, const FString& ComponentName)
	{
		AActor* Actor = Reference.Get();
		UParticleSubUVComponent* Component = Actor ? 
			Cast<UParticleSubUVComponent>(Actor->FindComponentByName(FName(ComponentName)))
			: nullptr;

		if (!Component)
		{
			throw sol::error("Particle component is unavailable: " + ComponentName);
		}

		return Component;
	}
	// 인수로 오는 state_view에 Vector를 바인딩하는 함수
	// TODO : 다른 공통 타입들도 바인딩해야함.
	static UCameraComponent* ResolveVehicleCamera(const TWeakObjectPtr<AActor>& Reference)
	{
		ARacingCar* Car = Cast<ARacingCar>(Reference.Get());
		UCameraComponent* Camera = Car ? Cast<UCameraComponent>(Car->FindComponentByName("VehicleCamera")) : nullptr;
		if (!Car || !Car->CanRunGameplay() || !Camera || Camera->IsPendingDestroy())
			throw sol::error("CameraFOV requires a RacingCar with an available VehicleCamera");
		return Camera;
	}

	void Register(sol::state_view Lua)
	{
		// FVector 바인딩
		Lua.new_usertype<FVector>("Vector",
			sol::call_constructor,
			sol::constructors<FVector(), FVector(float, float, float)>(),
			"x", &FVector::X,
			"y", &FVector::Y,
			"z", &FVector::Z,
			sol::meta_function::addition,
			[](const FVector& A, const FVector& B)
			{
				return A + B;
			},
			sol::meta_function::multiplication,
			[](const FVector& Value, float Scalar)
			{
				return Value * Scalar;
			},
			sol::meta_function::to_string,
			[](const FVector& Value)
			{
				return std::format("Vector({}, {}, {})", Value.X, Value.Y, Value.Z);
			},
			"Normalized", &FVector::Normalized	// 열거형에 없는 함수는 string,함수 포인터로 바인딩할수있음.
		);
		
		// FRotater 바인딩
		Lua.new_usertype<FRotator>("Rotator",
			sol::call_constructor,
			sol::constructors<FRotator(), FRotator(float, float, float)>(),
			"pitch", &FRotator::Pitch,
			"yaw", &FRotator::Yaw,
			"roll", &FRotator::Roll
		);
		// FHitResult 바인딩
		// 현재는 모든 필드에 대해 읽기/복사본 읽기만 가능.
		Lua.new_usertype<FHitResult>("HitResult",
			sol::no_constructor,
			"FaceIndex", sol::readonly(&FHitResult::FaceIndex),
			"Time", sol::readonly(&FHitResult::Time),
			"Distance", sol::readonly(&FHitResult::Distance),
			"PenetrationDepth", sol::readonly(&FHitResult::PenetrationDepth),
			"bBlockingHit", sol::readonly(&FHitResult::bBlockingHit),
			"bStartPenetrating", sol::readonly(&FHitResult::bStartPenetrating),
			"Location", sol::property([](const FHitResult& Hit) -> FVector
				{
					return Hit.Location;
				}),
			"ImpactPoint", sol::property([](const FHitResult& Hit) -> FVector
				{
					return Hit.ImpactPoint;
				}),
			"Normal", sol::property([](const FHitResult& Hit) -> FVector
				{
					return Hit.Normal;
				}),
			"ImpactNormal", sol::property([](const FHitResult& Hit) -> FVector
				{
					return Hit.ImpactNormal;
				}),
			"TraceStart", sol::property([](const FHitResult& Hit) -> FVector
				{
					return Hit.TraceStart;
				}),
			"TraceEnd", sol::property([](const FHitResult& Hit) -> FVector
				{
					return Hit.TraceEnd;
				})
		);
		// 충돌 상대 컴포넌트 WeakPtr 바인딩
		// 현재는 Name, IsValid 읽기만 가능
		Lua.new_usertype<TWeakObjectPtr<UPrimitiveComponent>>("PrimitiveComponent",
			sol::no_constructor,
			"Name", sol::property(
				[](const TWeakObjectPtr<UPrimitiveComponent>& Reference) -> FString
				{
					UPrimitiveComponent* Component = Reference.Get();
					if (!Component)
					{
						throw sol::error("Primitive component has been destroyed");
					}

					return Component->GetName();
				}),
			"IsValid", [](const TWeakObjectPtr<UPrimitiveComponent>& Reference) -> bool
			{
				UPrimitiveComponent* Component = Reference.Get();
				AActor* Owner = Component ? Component->GetOwner() : nullptr;

				return Component && !Component->IsPendingDestroy() &&
					Owner && !Owner->IsPendingDestroy();
			}
		);
		// AActor 바인딩
		Lua.new_usertype<TWeakObjectPtr<AActor>>("Actor",
			sol::no_constructor,
			"UUID", sol::property(
				[](const TWeakObjectPtr<AActor>& Reference) -> uint32
				{
					AActor* Actor = Reference.Get();
					if (!Actor)
					{
						throw sol::error("Actor has been destroyed");
					}

					return Actor->GetUUID();
				}),
			// Actor 이름 조회
			"Name", sol::property(
				[](const TWeakObjectPtr<AActor>& Reference) -> FString
				{
					AActor* Actor = Reference.Get();
					if (!Actor)
					{
						throw sol::error("Actor has been destroyed");
					}

					return Actor->GetName();
				}),
			// Label 이름 조회
			"Label", sol::property(
				[](const TWeakObjectPtr<AActor>& Reference) -> FString
				{
					AActor* Actor = Reference.Get();
					if (!Actor)
					{
						throw sol::error("Actor has been destroyed");
					}

					return Actor->GetActorLabel();
				}),
			"IsSwamp", [](const TWeakObjectPtr<AActor>& Reference) -> bool
				{
					return Cast<ASwamp>(Reference.Get()) != nullptr;
				},
			// 삭제 예약 상태를 포함한 유효성 판정
			"IsValid", [](const TWeakObjectPtr<AActor>& Reference) -> bool
				{
					AActor* Actor = Reference.Get();
					return Actor && !Actor->IsPendingDestroy();
				},
				"Destroy", [](const TWeakObjectPtr<AActor>& Reference) -> bool
				{
					AActor* Actor = Reference.Get();
					return Actor && Actor->Destroy();
				},
			"Location", sol::property(
				[](const TWeakObjectPtr<AActor>& Reference) -> FVector
				{
					// 루트 컴포넌트의 월드 위치 조회
					AActor* Actor = Reference.Get();
					USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
					if (!Root || Root->IsPendingDestroy())
					{
						throw sol::error("Actor root component is unavailable");
					}

					return Root->GetWorldLocation();
				},
				[](const TWeakObjectPtr<AActor>& Reference, const FVector& Location)
				{
					// 실행 가능한 루트 컴포넌트의 위치 변경
					AActor* Actor = Reference.Get();
					USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
					if (!Root || !Root->CanRunGameplay())
					{
						throw sol::error("Actor root component cannot move");
					}

					Root->SetWorldLocation(Location, false);
				}),
			"Rotation", sol::property(
				[](const TWeakObjectPtr<AActor>& Reference)->FRotator
				{
					AActor* Actor = Reference.Get();
					USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
					if (!Root || Root->IsPendingDestroy())
					{
						throw sol::error("Actor root component is unavailable");
					}

					return Root->GetWorldRotation();
				},
				[](const TWeakObjectPtr<AActor>& Reference,const FRotator& Rotation)
				{
					AActor* Actor = Reference.Get();
					USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
					if (!Root || !Root->CanRunGameplay())
					{
						throw sol::error("Actor root component cannot rotate");
					}

					return Root->SetWorldRotation(Rotation,false);
				}),
			// 파티클 관련 바인딩
			// 지정한 컴포넌트의 Burst 재생
			"EmitParticleBurst", [](
				const TWeakObjectPtr<AActor>& Reference, const FString& ComponentName,
				const FVector& Position, const FVector& Direction, int32 Count) -> int32
			{
				return ResolveParticleComponent(Reference, ComponentName)
					->EmitBurst(Position, Direction, Count);
			},

			// 지정한 컴포넌트의 활성 입자 정리
			"ClearParticles", [](
				const TWeakObjectPtr<AActor>& Reference, const FString& ComponentName)
			{
				ResolveParticleComponent(Reference, ComponentName)->ClearParticles();
			},
			// Character 관련 바인딩
			// AActor상속받는 구조->바인딩된 obj에 대해 Character인지 확인하는 절차를 거침.
			"Velocity", sol::property(
				[](const TWeakObjectPtr<AActor>& Reference) -> FVector
				{
					ACharacter* Character = Cast<ACharacter>(Reference.Get());
					if (!Character)
					{
						throw sol::error("Velocity requires a Character actor");
					}

					return Character->GetVelocity();
				},
				[](const TWeakObjectPtr<AActor>& Reference, const FVector& Velocity)
				{
					ACharacter* Character = Cast<ACharacter>(Reference.Get());
					if (!Character)
					{
						throw sol::error("Velocity requires a Character actor");
					}

					if (!Character->CanRunGameplay())
					{
						throw sol::error("Character cannot change velocity");
					}

					Character->SetVelocity(Velocity);
				}),
			"FogEnabled", sol::property(
				[](const TWeakObjectPtr<AActor>& Reference) -> bool
				{
					ASwamp* Swamp = Cast<ASwamp>(Reference.Get());
					if (!Swamp || !Swamp->GetFogComponent()) throw sol::error("FogEnabled requires a Swamp with a Fog component");
					return Swamp->GetFogComponent()->IsFogEnabled();
				},
				[](const TWeakObjectPtr<AActor>& Reference, bool bEnabled)
				{
					ASwamp* Swamp = Cast<ASwamp>(Reference.Get());
					if (!Swamp || !Swamp->GetFogComponent() || Swamp->GetFogComponent()->IsPendingDestroy()) throw sol::error("Swamp Fog component is unavailable");
					Swamp->GetFogComponent()->SetFogEnabled(bEnabled);
				}),
			"CameraFOV", sol::property(
				[](const TWeakObjectPtr<AActor>& Reference) -> float
				{
					return ResolveVehicleCamera(Reference)->GetFieldOfView();
				},
				[](const TWeakObjectPtr<AActor>& Reference, float FieldOfView)
				{
					ResolveVehicleCamera(Reference)->SetFieldOfView(FieldOfView);
				}),
			"MaxSpeed", sol::property(
				[](const TWeakObjectPtr<AActor>& Reference) -> float
				{
					ARacingCar* Car = Cast<ARacingCar>(Reference.Get());
					if (!Car)
					{
						throw sol::error("MaxSpeed requires a RacingCar actor");
					}
					return Car->GetMaxSpeed();
				})
		);

		// Tostring 바인딩
		const sol::protected_function ToString = Lua["tostring"];

		// Print 바인딩
		Lua.set_function("print", [ToString](sol::variadic_args Args)
			{
				FString Message;

				for (int Index = 0; Index < Args.leftover_count(); ++Index)
				{
					// Coroutine에서 호출시 Lua참조로 전달
					const sol::object Value = Args.get<sol::object>(Index);
					sol::protected_function_result Result = ToString(Value);

					// 변환 오류 보호된 실행으로 전달해 Runtime에서 기록
					if (!Result.valid())
					{
						const sol::error Error = Result;
						throw Error;
					}

					if (Index > 0)
						Message += '\t';

					Message += Result.get<FString>();
				}

				HTR_LOG(Info, "[Lua] {}", Message);
			}
		);

		// 생략한 재생 옵션은 기본값으로 처리합니다.
		sol::table Audio = Lua.create_named_table("audio");
		auto PlaySound = [](const FString& FilePath, sol::optional<bool> bLoop, sol::optional<float> Volume) -> int64
		{
			assert(GEngine != nullptr);

			FSoundManager* SoundManager = GEngine->GetSoundManager();
			assert(SoundManager != nullptr);

			const bool bShouldLoop = bLoop.value_or(false);
			const float SoundVolume = Volume.value_or(1.0f);

			// Lua에서 잘못된 볼륨을 전달하면 재생하지 않습니다.
			if (!(SoundVolume >= 0.0f && SoundVolume <= 1.0f))
			{
				// 재생 실패는 음수로 반환합니다.
				return -1;
			}

			return SoundManager->PlaySound(FilePath, bShouldLoop, SoundVolume);
		};
		Audio.set_function("play", PlaySound);

		// 같은 파일의 다른 재생에는 영향을 주지 않습니다.
		Audio.set_function("stop", [](int64 SoundId)
		{
			return GEngine->GetSoundManager()->StopSound(SoundId);
		});

		Audio.set_function("set_volume", [](int64 SoundId, float Volume)
		{
			// Lua에서 전달한 볼륨은 범위를 검사한 뒤 사용합니다.
			if (!(Volume >= 0.0f && Volume <= 1.0f))
			{
				return false;
			}

			return GEngine->GetSoundManager()->SetSoundVolume(SoundId, Volume);
		});

		Audio.set_function("set_pitch", [](int64 SoundId, float Pitch)
		{
			// 피치 범위 보정은 사운드 매니저에서 C++ 호출과 동일하게 처리합니다.
			return GEngine->GetSoundManager()->SetSoundPitch(SoundId, Pitch);
		});

		// BGM 볼륨을 생략하면 기본값 1.0을 사용합니다.
		auto PlayBGM = [](const FString& FilePath, sol::optional<float> Volume)
		{
			const float SoundVolume = Volume.value_or(1.0f);
			if (!(SoundVolume >= 0.0f && SoundVolume <= 1.0f))
			{
				return false;
			}

			return GEngine->GetSoundManager()->PlayBGM(FilePath, SoundVolume);
		};
		Audio.set_function("play_bgm", PlayBGM);

		auto SetBGMVolume = [](float Volume)
		{
			// 범위를 벗어난 입력은 현재 볼륨을 변경하지 않습니다.
			if (!(Volume >= 0.0f && Volume <= 1.0f))
			{
				return false;
			}

			return GEngine->GetSoundManager()->SetBGMVolume(Volume);
		};
		Audio.set_function("set_bgm_volume", SetBGMVolume);

		// BGM 재생을 종료하고 음원 자원도 해제합니다.
		Audio.set_function("stop_bgm", []()
		{
			GEngine->GetSoundManager()->StopBGM();
		});

		// true는 일시정지, false는 재개입니다.
		Audio.set_function("pause_bgm", [](bool bPaused)
		{
			return GEngine->GetSoundManager()->SetBGMPaused(bPaused);
		});

		// 인풋 관련 함수 바인딩
		sol::table Input = Lua.create_named_table("input");
		Input.set_function("is_down", [](const FString& Key)
			{
				return FInputSystem::IsKeyDown(ResolveInputKey(Key));
			}
		);
		Input.set_function("is_pressed", [](const FString& Key)
			{
				return FInputSystem::IsKeyPressed(ResolveInputKey(Key));
			}
		);
		Input.set_function("is_released", [](const FString& Key)
			{
				return FInputSystem::IsKeyReleased(ResolveInputKey(Key));
			}
		);

		sol::table Game = Lua.create_named_table("game");
		Game.set_function("get_score", []() -> int32
			{
				return FGameHud::GetScore();
			}
		);
		Game.set_function("set_score", [](const int32 num)
			{
				FGameHud::SetScore(num);
			}
		);
		Game.set_function("set_countdown", [](const FString& Text)
			{
				FGameHud::SetCountdown(Text);
			}
		);
		Game.set_function("set_race_time", [](const float Seconds)
			{
				FGameHud::SetRaceTime(Seconds);
			}
		);

		Game.set_function("update_best_records", [](int32 Score, double FinishTime)
			{
				// 파일이 없는 첫 완주에서는 이번 결과를 최고 기록으로 사용합니다.
				int32 BestScore = Score;
				double BestTime = FinishTime;

				{
					std::ifstream File("Leaderboard.save");
					int32 SavedScore = 0;
					double SavedTime = 0.0;
					File >> SavedScore >> SavedTime;

					const bool bReadSucceeded = !File.fail();
					const bool bValidScore = SavedScore >= 0;
					const bool bValidTime = SavedTime > 0.0;

					if (bReadSucceeded && bValidScore && bValidTime)
					{
						// 최고 점수와 최단 시간은 서로 다른 주행일 수 있습니다.
						BestScore = std::max(BestScore, SavedScore);
						BestTime = std::min(BestTime, SavedTime);
					}
				}

				// 읽기용 파일이 닫힌 뒤 갱신한 기록을 저장합니다.
				std::ofstream File("Leaderboard.save");
				File << BestScore << '\n'
					 << std::setprecision(17) << BestTime << '\n';
				File.close();

				return std::make_tuple(BestScore, BestTime, !File.fail());
			}
		);

		Game.set_function("set_race_result",
			[](int32 FinishScore, double FinishTime,
				int32 BestScore, double BestFinishTime)
			{
				// 결과 화면이 열리기 전에 Lua의 확정 기록을 C++에 복사합니다.
				FRaceResult Result;
				Result.FinishScore = FinishScore;
				Result.FinishTime = FinishTime;
				Result.BestScore = BestScore;
				Result.BestFinishTime = BestFinishTime;
				FGameHud::SetRaceResult(Result);
			}
		);

		Game.set_function("show_finish_ui", []()
			{
				FTimeDilation::StartSlomo(0.3f, 0.1f, 2.0f, 0.8f);
				FGameHud::SetResultScreen(ERaceResultScreen::Finish);
			}
		);
		Game.set_function("play_intro_sequence", []()
			{
				TArray<FSequenceStep> Steps;
				Steps.Push({ 3.0f , ESequenceAction::Wait });
				FSequencePlayer::Play(Steps);
			}
		);
		Game.set_function("is_sequence_playing", []()
			{
				return FSequencePlayer::IsPlaying();
			}
		);
		Game.set_function("reset_result_ui", []()
			{
				FGameHud::SetResultScreen(ERaceResultScreen::Hidden);
				FTimeDilation::ResetDilation();
			}
		);
	}
}
