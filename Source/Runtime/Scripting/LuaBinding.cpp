#include "EnginePCH.h"
#include "Scripting/LuaBindings.h"
#include "Math/Vector.h"
#include <format>

namespace LuaBindings
{
	// 인수로 오는 state_view에 Vector를 바인딩하는 함수
	// TODO : 다른 공통 타입들도 바인딩해야함.
	void Register(sol::state_view Lua)
	{
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

		const sol::protected_function ToString = Lua["tostring"];
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
			});
	}
}