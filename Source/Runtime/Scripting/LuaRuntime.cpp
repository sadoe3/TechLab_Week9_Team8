#include "EnginePCH.h"
#include "Scripting/LuaRuntime.h"

// 기본 라이브러리만 준비하며 게임 객체는 아직 노출하지 않습니다.
FLuaRuntime::FLuaRuntime()
{
	Lua.open_libraries(
		sol::lib::base,
		sol::lib::math,
		sol::lib::table,
		sol::lib::string,
		sol::lib::coroutine);
}

// sol::state가 소멸하면서 VM을 해제합니다.
FLuaRuntime::~FLuaRuntime() = default;

// 일반 전역 변수와 _G 쓰기를 각 환경에 보관합니다.
sol::environment FLuaRuntime::CreateEnvironment()
{
	sol::environment Environment(Lua, sol::create, Lua.globals());
	Environment["_G"] = Environment;
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
