#include "EnginePCH.h"
#include "Editor/HitoriEd/LuaScriptEditorUtils.h"

#include "Core/EngineLog.h"
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

FString FLuaScriptEditorUtils::MakeValidFileName(const FString& Name)
{
	// 원본 이름은 유지하고 복사한 문자열만 수정한다.
	FString Result = Name;
	const FString InvalidCharacters = "<>:\"/\\|?*";

	for (char& Character : Result)
	{
		// Windows 파일명에 사용할 수 없는 문자와 제어 문자를 바꾼다.
		if (static_cast<unsigned char>(Character) < 32 || InvalidCharacters.find(Character) != FString::npos)
		{
			Character = '_';
		}
	}

	return Result;
}

bool FLuaScriptEditorUtils::CreateLuaScript(const FString& SceneName, const FString& ActorName, FString& OutScriptPath)
{
	// 파일명을 만들 수 있도록 Scene·Actor 이름을 먼저 확인한다.
	if (SceneName.empty() || ActorName.empty())
	{
		HTR_LOG(Error, "Scene and Actor names are required.");
		return false;
	}

	const FString FileName = MakeValidFileName(SceneName) + "_" + MakeValidFileName(ActorName) + ".lua";
	const fs::path TemplatePath = "Assets/Scripts/template.lua";
	const fs::path ScriptPath = fs::path("Assets/Scripts") / FileName;

	// 출력 폴더가 없으면 생성한다. 이미 존재하면 그대로 사용한다.
	std::error_code Error;
	fs::create_directories(ScriptPath.parent_path(), Error);
	if (Error)
	{
		HTR_LOG(Error, "Cannot create script folder: {}", Error.message());
		return false;
	}

	// 템플릿을 그대로 복사한다. 같은 파일이 이미 있으면 덮어쓰지 않는다.
	if (!fs::copy_file(TemplatePath, ScriptPath, fs::copy_options::none, Error))
	{
		HTR_LOG(Error, "Cannot create Lua script ({}): {}", FileName, Error.message());
		return false;
	}

	// 복사 성공 시에만 연결할 경로를 변경하고 구분자를 '/'로 통일한다.
	OutScriptPath = ScriptPath.generic_string();
	HTR_LOG(Info, "Created Lua script: {}", OutScriptPath);
	return true;
}

bool FLuaScriptEditorUtils::EditLuaScript(const FString& ScriptPath)
{
	// 프로젝트 상대 경로를 절대 경로로 바꾸고 파일 존재를 확인한다.
	std::error_code Error;
	const fs::path FilePath = fs::absolute(ScriptPath, Error);
	if (ScriptPath.empty() || Error || !fs::is_regular_file(FilePath, Error))
	{
		HTR_LOG(Error, "Lua script file is unavailable: {}", ScriptPath);
		return false;
	}

	SHELLEXECUTEINFOW ExecuteInfo = {};
	ExecuteInfo.cbSize = sizeof(ExecuteInfo);
	ExecuteInfo.hwnd = GetActiveWindow();
	ExecuteInfo.fMask = SEE_MASK_FLAG_NO_UI;

	// Lua 실행기가 연결된 '열기' 대신 '편집' 명령을 먼저 사용한다.
	ExecuteInfo.lpVerb = L"edit";
	ExecuteInfo.lpFile = FilePath.c_str();
	ExecuteInfo.nShow = SW_SHOWNORMAL;
	if (ShellExecuteExW(&ExecuteInfo))
	{
		return true;
	}

	// 편집 명령으로 열지 못하면 메모장을 사용한다. 공백이 있는 경로도 보호한다.
	const std::wstring Parameters = L"\"" + FilePath.wstring() + L"\"";
	ExecuteInfo.lpVerb = L"open";
	ExecuteInfo.lpFile = L"notepad.exe";
	ExecuteInfo.lpParameters = Parameters.c_str();
	if (!ShellExecuteExW(&ExecuteInfo))
	{
		HTR_LOG(Error, "Cannot open Lua editor ({}), error {}", ScriptPath, GetLastError());
		return false;
	}

	// 편집기 실행 요청의 성공이며, 편집 내용 저장을 뜻하지 않는다.
	return true;
}
