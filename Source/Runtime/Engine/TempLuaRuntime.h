#pragma once

#include <sol/sol.hpp>

#include "GameFramework/Actor.h"

// Todo: Script component
class FTempLuaRuntime
{
public:
    FTempLuaRuntime()
    {
        Lua.open_libraries(sol::lib::base);

        // Lua print의 인자를 모아서 엔진 로그 한 건으로 출력한다.
        Lua.set_function(
            "print", 
            [this](sol::variadic_args Arguments)
            {
                FString Message;
                sol::protected_function ToString = Lua["tostring"];

                for (size_t Index = 0; Index < Arguments.size(); ++Index)
                {
                    // 문자열, 숫자, bool, nil 등을 Lua 방식으로 변환한다.
                    sol::protected_function_result Result = ToString(Arguments[Index]);
                    if (!Result.valid())
                    {
                        sol::error Error = Result;
                        HTR_LOG(Error, "Lua print failed: {}", Error.what());
                    
                        return;
                    }

                    // 기본 print처럼 인자 사이에 탭을 넣는다.
                    if (Index > 0)
                    {
                        Message += '\t';
                    }

                    Message += Result.get<FString>();
                }

                // 기존 로그 시스템이 Output Log 패널로 전달한다.
                HTR_LOG(Info, "[Lua] {}", Message);
            }
        );
    }

    sol::environment CreateEnvironment(AActor* OwnerActor)
    {
        sol::environment Environment(Lua, sol::create, Lua.globals());
        Environment["obj"] = OwnerActor;

        return Environment;
    }

    bool LoadScript(const FString& ScriptPath, const sol::environment& Environment)
    {
        sol::protected_function_result Result = Lua.safe_script_file(ScriptPath, Environment, sol::script_pass_on_error);

        if (!Result.valid())
        {
            sol::error Error = Result;
            HTR_LOG(Error, "Lua load failed ({}): {}", ScriptPath, Error.what());

            return false;
        }

        return true;
    }

private:
    sol::state Lua;
}; 