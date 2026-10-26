#include "EnginePCH.h"
#include "Engine.h"
#include "Launch/LaunchEngineLoop.h"
#include "Serialization/JsonArchive.h"
#include <utility>

UEngine* GEngine = nullptr;

bool UEngine::Init()
{
	/*World = FObjectFactory::ConstructObject<UWorld>();

	if (!World || !World->Init()) return false;*/

 	return true;
}

FSoundManager* UEngine::GetSoundManager() const
{
	return EngineLoop->GetSoundManager();
}

FWorldContext& UEngine::CreateNewWorldContext(EWorldType WorldType)
{
	FWorldContext* NewWorldContext = new FWorldContext();
	WorldList.Add(NewWorldContext);
	NewWorldContext->WorldType = WorldType;
	NewWorldContext->ContextHandle = FName();

	return *NewWorldContext;
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* InWorld)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.World() == InWorld)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

void UEngine::DestroyWorldContext(UWorld* InWorld)
{
	for (int32 idx = 0; idx < WorldList.Num(); ++idx)
	{
		if (WorldList[idx].World() == InWorld)
		{
			//WorldContextDestroyedEvent.Broadcast(WorldList[idx]);

			WorldList[idx].SetCurrentWorld(NULL);
			WorldList.RemoveAt(idx);
			break;
		}
	}
}

bool UEngine::RequestSceneChange(UWorld* World, const FString& ScenePath)
{
	// World 종료 중 예약 차단
	if (!World || World->bShutdown || World->bShuttingDown
		|| World->PlayState == EWorldPlayState::Ending)
	{
		return false;
	}

	FWorldContext* Context = GetWorldContextFromWorld(World);
	if (!Context)
	{
		return false;
	}

	// 마지막 요청으로 교체
	Context->PendingScenePath = ScenePath;
	Context->bSceneChangeRequested = true;
	return true;
}

bool UEngine::RequestExit(UWorld* World)
{
	// 게임월드에 대해서만 종료함수 호출, 실행 루프 검증
	if (!World || World->GetWorldType() != EWorldType::Game || !World->AcceptsChanges()
		|| !GetWorldContextFromWorld(World) || !EngineLoop)
	{
		return false;
	}

	EngineLoop->RequestExit();
	return true;
}

bool UEngine::ConsumeSceneChangeRequest(UWorld* World, FString& OutScenePath)
{
	FWorldContext* Context = GetWorldContextFromWorld(World);
	if (!Context || !Context->bSceneChangeRequested)
	{
		return false;
	}

	// 적용 중 발생할 새 요청과 분리
	OutScenePath = std::move(Context->PendingScenePath);
	Context->bSceneChangeRequested = false;
	return true;
}

bool UEngine::ApplySceneChange(UWorld* World, const FString& ScenePath)
{
	// 삭제 배치·World 정리 중 적용 차단
	if (!World || World->bFlushingPendingDestroy || !World->AcceptsChanges())
	{
		return false;
	}

	// 빈 경로의 새 씬 처리
	if (ScenePath.empty())
	{
		return World->ClearWorld();
	}

	// 기존 동기 로더 사용
	if (!FJsonArchive::LoadWorld(World, ScenePath))
	{
		HTR_LOG(Error, "Failed to change scene: {}", ScenePath);
		return false;
	}

	HTR_LOG(Info, "Load Scene : {}", ScenePath);
	return true;
}

void FWorldContext::SetCurrentWorld(UWorld* World)
{
	// 동일 World의 예약 유지
	if (ThisCurrentWorld == World)
	{
		return;
	}

	// 이전 World의 예약 폐기
	ThisCurrentWorld = World;
	bSceneChangeRequested = false;
	PendingScenePath.clear();
}

FWorldContext::FWorldContext() : WorldType(EWorldType::None), ContextHandle(NAME_None) {}
FWorldContext::~FWorldContext() = default;
