#include "EnginePCH.h"
#include "GameEngine.h"

#include "Launch/LaunchEngineLoop.h"
#include "Serialization/JsonArchive.h"

FEngineConfig UGameEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Hitori";
	Desc.Width = 0;
	Desc.Height = 0;
	Desc.bBorderless = true;
	Desc.SyncInterval = 0;   // 0 = VSync 끔 (FPS 측정용)
	Desc.bCreateDepthBuffer = true;   
	Desc.bExitOnEscape = true;        
	return Desc;
}

bool UGameEngine::Init()
{
	if (!Super::Init())
	{
		return false;
	}

	// 게임 World와 Context 연결
	GameWorld = UWorld::CreateWorld(EWorldType::Game, false);
	CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(GameWorld);

	// 시작 씬 로드
	// TODO: 시작 씬을 게임의 시작으로 옮겨야함
	const FString StartupScenePath = "Assets/Scenes/LinearColorTest.scene";
	if (!FJsonArchive::LoadWorld(GameWorld, StartupScenePath))
	{
		HTR_LOG(Error, "Failed to load startup scene: {}", StartupScenePath);
		PreExit();
		return false;
	}

	// 게임 생명주기 시작
	GameWorld->BeginPlay();
	return true;
}

void UGameEngine::PreTick(float DeltaTime)
{
	// 프레임 시작 씬 전환
	FString ScenePath;
	if (ConsumeSceneChangeRequest(GameWorld, ScenePath))
	{
		ApplySceneChange(GameWorld, ScenePath);
	}
}

void UGameEngine::Tick(float DeltaTime)
{
	// 게임 World 갱신
	GameWorld->Tick(DeltaTime);
}

void UGameEngine::PostTick(float DeltaTime)
{
	// 프레임 종료 삭제 처리
	GameWorld->FlushPendingDestruction(GameWorld->GetPendingDestroyCount());
}

void UGameEngine::PreExit()
{
	// World 종료와 Context 해제
	if (GameWorld)
	{
		GameWorld->Shutdown();
		DestroyWorldContext(GameWorld);
		GameWorld = nullptr;
	}

	Super::PreExit();
}