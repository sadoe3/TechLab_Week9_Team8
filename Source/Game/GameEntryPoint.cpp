#include "EnginePCH.h"
#include "Engine/GameEngine.h"
#include "Launch/EntryPoint.h"

// 게임 실행 엔진 선택
UClass* GetEngineClass()
{
	return UGameEngine::StaticClass();
}