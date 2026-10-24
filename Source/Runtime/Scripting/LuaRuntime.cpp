#include "EnginePCH.h"
#include "Scripting/LuaRuntime.h"
#include "Scripting/LuaBindings.h"
#include "GameFramework/Actor.h"
#include "Component/ScriptComponent.h"
#include "Containers/Array.h"
#include "Engine/World.h"

// 기본 라이브러리 준비 후 공통 C++ 바인딩
FLuaRuntime::FLuaRuntime()
{
	Lua.open_libraries(
		sol::lib::base,
		sol::lib::math,
		sol::lib::table,
		sol::lib::string,
		sol::lib::coroutine);

	LuaBindings::Register(Lua);
}

// sol::state가 소멸하면서 VM을 해제합니다.
FLuaRuntime::~FLuaRuntime() = default;

// 일반 전역 변수와 _G 쓰기를 각 환경에 보관합니다.
sol::environment FLuaRuntime::CreateEnvironment(AActor* OwnerActor)
{
	sol::environment Environment(Lua, sol::create, Lua.globals());
	Environment["_G"] = Environment;
	Environment["obj"] = TWeakObjectPtr<AActor>(OwnerActor);
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

	sol::call_status Status;
	{
		// 결과 정리 범위
		sol::protected_function_result Result = Entry->Function();
		Status = Result.status();
		CheckResult(Result, Entry->ScriptPath, "ResumeCoroutine");
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
