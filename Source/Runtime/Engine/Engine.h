#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

#include "Containers/IndirectArray.h"

#include "Engine/World.h"
#include "EngineTypes.h"

#include "Asset/AssetManager.h"

struct FWorldContext
{
	EWorldType WorldType;
	FName ContextHandle;
	// Renderer 소유 World별 그림자 자원 슬롯의 비소유 번호. 저장/복제하지 않는다.
	uint32 ShadowResourceIndex = InvalidShadowResourceIndex;

	// 마지막 씬 전환 요청
	bool bSceneChangeRequested = false;
	FString PendingScenePath;

	void SetCurrentWorld(UWorld* World);

	inline UWorld* World() const
	{
		return ThisCurrentWorld;
	}

	FWorldContext();
	~FWorldContext();
	// WorldList는 포인터만 이동한다. 세션 자체의 복사/이동은 허용하지 않는다.
	FWorldContext(const FWorldContext&) = delete;
	FWorldContext& operator=(const FWorldContext&) = delete;
	FWorldContext(FWorldContext&&) = delete;
	FWorldContext& operator=(FWorldContext&&) = delete;

private:
	UWorld* ThisCurrentWorld = nullptr;
};

class FEngineLoop;

// 엔진 모드별 실행 설정. UE에서는 ini가 맡는 부분이다.
// Width·Height가 0이면 주 모니터 해상도를 쓴다.
struct FEngineConfig
{
	const wchar_t* Title = L"Hitori";
	uint32 Width = 0;
	uint32 Height = 0;
	bool bBorderless = true;
	uint32 SyncInterval = 0;   // 0 = VSync 끔 (FPS 측정용)
	bool bCreateDepthBuffer = true;   // 백버퍼에 3D를 직접 그리지 않는 엔진(에디터)은 끈다.
	bool bExitOnEscape = true;        // 테두리 없는 창에는 닫기 버튼이 없다.
	const char* SplashImage = nullptr;
};

// 모든 엔진 모드의 베이스. 플랫폼·루프·Present는 FEngineLoop가 맡고,
// 서브클래스(Game, Editor)는 Tick에서 한 프레임의 갱신과 렌더를 채운다.
class UEngine : public UObject
{
	DECLARE_CLASS(UEngine, UObject)
	friend class FEngineLoop;
public:
	// 창을 만들기 전에 호출된다.
	virtual FEngineConfig GetConfig() const { return {}; }

	// Device·Window·Swapchain·AssetManager가 준비된 뒤 호출된다.
	virtual bool Init();
	// 프레임 시작 요청 처리
	virtual void PreTick(float) {}
	// 한 프레임의 갱신과 모든 렌더 패스를 기록한다. Present는 FEngineLoop가 호출 직후에 한다.
	virtual void Tick(float DeltaTime) = 0;
	// World 비소유 참조 정리
	virtual void ReleaseWorldReferences(UWorld*) {}
	// 프레임 종료 요청 처리
	virtual void PostTick(float) {}
	virtual void OnResize(uint32 Width, uint32 Height) {}
	// UObject 일괄 정리와 GPU 자원 해제 전에 호출된다.
	virtual void PreExit() {};

	//UWorld* GetWorld() const { return World; }

	FWorldContext& CreateNewWorldContext(EWorldType WorldType);
	FWorldContext* GetWorldContextFromWorld(const UWorld* InWorld);
	void DestroyWorldContext(UWorld* InWorld);

	// 씬 전환 예약, 빈 경로는 새 씬
	bool RequestSceneChange(UWorld* World, const FString& ScenePath);
	
	UAssetManager* AssetManager;
protected:
	FEngineLoop& GetEngineLoop() const { return *EngineLoop; }

	// 프레임 시작 요청 수집
	bool ConsumeSceneChangeRequest(UWorld* World, FString& OutScenePath);

	// 프레임 시작 전용 적용
	bool ApplySceneChange(UWorld* World, const FString& ScenePath);

	TIndirectArray<FWorldContext> WorldList;
	//UWorld* World = nullptr;
	int32 NextWorldContextHandle = 0;
private:
	FEngineLoop* EngineLoop = nullptr;
};

// FEngineLoop::PreInit에서 설정되고 Exit에서 해제된다.
extern UEngine* GEngine;
