#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Engine/EngineBaseTypes.h"

class AActor;

class UActorComponent : public UObject
{
	DECLARE_CLASS(UActorComponent, UObject)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	UActorComponent() { PrimaryComponentTick.Target = this; }
	virtual ~UActorComponent() override;

	void RegisterComponent();
	void UnregisterComponent();
	bool IsRegistered() const { return bRegistered; }

	virtual void InitializeComponent() {};
	virtual void UninitializeComponent() {};
	virtual void BeginPlay() {};
	virtual void EndPlay() {};
	virtual void TickComponent(float DeltaTime) 
	{
		//HTR_LOG(Info, "TickComponent");
	};

	void SetOwner(AActor* InOwner) { Owner = InOwner; }
    AActor* GetOwner() const { return Owner; }

	// 디테일 패널에서 이 컴포넌트를 통째로 숨긴다 (에디터 시각화용 빌보드 등)
	void SetHiddenInDetails(bool bInHidden) { bHiddenInDetails = bInHidden; }
	bool IsHiddenInDetails() const { return bHiddenInDetails; }

	// 게임 플레이(PIE) 중에 이 컴포넌트를 숨길지 여부
	void SetHiddenInGame(bool bInHidden) { bHiddenInGame = bInHidden; }
	bool IsHiddenInGame() const { return bHiddenInGame; }

	// UE와 같이 기본값은 bCanEverTick = false. Tick이 필요한 컴포넌트만 생성자에서 켠다.
	FActorComponentTickFunction PrimaryComponentTick;

	virtual void SetActive(bool bNewActive);
	bool IsActive() const { return bIsActive; }

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record);

	bool IsInitialized() const { return bInitialized; }
	bool HasBegunPlay() const { return bHasBegunPlay; }
	bool IsPendingDestroy() const { return bPendingDestroy; }
	bool CanRunGameplay() const;

protected:
	// 씬 연결시 등록 자원 준비
	virtual void OnRegister() {}
	// 씬 연결 해제 후 자원 정리
	virtual void OnUnregister() {}

	// 공통 함수를 쓰기 위한 렌더 훅
	virtual void CreateRenderState() {}
	virtual void DestroyRenderState() {}
private:
	friend class AActor;
	friend class UWorld;

	AActor* Owner = nullptr;
	bool bHiddenInDetails = false;
	bool bHiddenInGame = false;
	bool bIsActive = true;
	bool bRegistered = false;
	

	// 엔진은 비가상 진입점, 내부는 가상 훅을 호출
	void DispatchInitializeComponent();
	void DispatchBeginPlay();
	void DispatchEndPlay();
	void DispatchUninitializeComponent();

	// 중복 초기화/시작/정지/삭제를 막는 변수
	bool bInitialized = false;
	bool bInitializing = false;
	bool bHasBegunPlay = false;
	bool bBeginningPlay = false;
	bool bStopping = false;
	bool bPendingDestroy = false;
};