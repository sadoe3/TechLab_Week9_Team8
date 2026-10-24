#pragma once

#include "Core/EngineString.h"

class FLuaScriptEditorUtils
{
public:
	// template.lua를 SceneName_ActorName.lua로 복사한다.
	// 성공한 경우에만 상대 경로를 반환하고 기존 파일은 덮어쓰지 않는다.
	static bool CreateLuaScript(const FString& SceneName, const FString& ActorName, FString& OutScriptPath);

	// 연결된 Lua 파일을 외부 텍스트 편집기로 연다.
	static bool EditLuaScript(const FString& ScriptPath);

private:
	// 파일명으로 사용할 수 없는 문자를 '_'로 바꾼다.
	static FString MakeValidFileName(const FString& Name);
};
