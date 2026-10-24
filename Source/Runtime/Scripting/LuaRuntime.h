#pragma once

#include "Core/EngineString.h"
#include <sol/sol.hpp>
#include <utility>
#include "Containers/Map.h"

// Envirment 소유 액터
class AActor;

// Lua VM을 소유하고 환경 생성과 보호된 실행을 제공합니다.
class FLuaRuntime
{
public:
	FLuaRuntime();
	~FLuaRuntime();

	// VM 소유권과 등록 참조가 복사되지 않도록 제한합니다.
	FLuaRuntime(const FLuaRuntime&) = delete;
	FLuaRuntime& operator=(const FLuaRuntime&) = delete;

	// 공통 라이브러리를 조회할 수 있는 새 환경을 생성합니다.
	sol::environment CreateEnvironment(AActor* OwnerActor);

	// 파일을 지정 환경에서 실행하고 성공 여부를 반환합니다.
	bool LoadScript(
		const FString& ScriptPath, sol::environment& Environment);

	// 선택적 이벤트의 존재 여부는 호출자가 판단합니다.
	template<typename... TArgs>
	bool Call(const sol::protected_function& Function,
		const FString& ScriptPath, const char* EventName, TArgs&&... Args)
	{
		if (!Function.valid())
		{
			LogError(ScriptPath, EventName, "Invalid Lua function");
			return false;
		}

		// 결과는 이 호출 안에서 검사하고 정리합니다.
		sol::protected_function_result Result =
			Function(std::forward<TArgs>(Args)...);

		return CheckResult(Result, ScriptPath, EventName);
	}

	// 코루틴 생성 함수
	uint32 CreateCoroutine(const sol::protected_function& Function, const FString& ScriptPath);
	// 코루틴 실행 재개
	sol::call_status ResumeCoroutine(uint32 CoroutineID);
private:
	bool CheckResult(const sol::protected_function_result& Result,
		const FString& ScriptPath, const char* EventName) const;

	// 파일명과 이벤트명을 포함한 오류 형식을 한곳에서 관리합니다.
	void LogError(const FString& ScriptPath,
		const char* EventName, const char* Message) const;

	// Coroutine 실행 상태
	struct FLuaCoroutine
	{
		sol::thread Thread;
		sol::coroutine Function;
		FString ScriptPath;
	};

	// 실행 상태보다 늦게 해제할 VM
	sol::state Lua;

	// ID별 실행 상태
	TMap<uint32, FLuaCoroutine> Coroutines;
	uint32 NextCoroutineID = 1;
};
