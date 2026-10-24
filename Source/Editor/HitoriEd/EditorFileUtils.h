#pragma once

#include "Engine/World.h"

class FEditorFileUtils
{
public:
	static bool NewScene(UWorld* World);
	static bool SaveScene(UWorld* World);
	static bool SaveSceneAs(UWorld* World);
	static bool LoadScene(UWorld* World);

	// 적용 완료 후 저장 경로 갱신
	static void SetCurrentScenePath(const FString& ScenePath)
	{
		CurrentScenePath = ScenePath;
	}

private:
	static FString OpenSaveSceneDialog();
	static FString OpenLoadSceneDialog();

	static FString CurrentScenePath;
};

