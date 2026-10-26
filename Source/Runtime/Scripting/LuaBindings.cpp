#include "EnginePCH.h"
#include "Scripting/LuaBindings.h"
#include "Math/Vector.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtr.h"
#include <format>
#include "Component/SceneComponent.h"
#include "Input/InputSystem.h"
#include "Math/Rotator.h"
#include "GameFramework/Character.h"
#include "GameFramework/RacingCar.h"
#include "GameFramework/Swamp.h"
#include "Component/ExponentialHeightFogComponent.h"

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

	// 인수로 오는 state_view에 Vector를 바인딩하는 함수
	// TODO : 다른 공통 타입들도 바인딩해야함.
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
	}
}
