#include "EnginePCH.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

UActorComponent::~UActorComponent()
{
    if (Owner)
    {
        Owner->RemoveOwnedComponent(this);
    }
}

void UActorComponent::SetActive(bool bNewActive)
{
    bIsActive = bNewActive;
}

void UActorComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	FArchive& Ar = Record.GetUnderlyingArchive();
	if (Ar.HasAnyPortFlags(EPropertyPortFlags::PPF_Duplicate))
	{
		UObject* OwnerObj = Owner;
		Record << SA_VALUE("Owner", OwnerObj);
		Owner = static_cast<AActor*>(OwnerObj);
	}
}

void UActorComponent::RegisterComponent()
{
	if (bRegistered || !Owner)
		return;

	UWorld* World = Owner->GetWorld();
	if (!World)
		return;

	bRegistered = true;
	OnRegister();
	CreateRenderState();
}

void UActorComponent::UnregisterComponent()
{
	if (!bRegistered)
		return;

	bRegistered = false;
	DestroyRenderState();
	OnUnregister();
}

bool UActorComponent::CanRunGameplay() const
{
	return bRegistered && !bPendingDestroy && !bStopping && Owner && Owner->CanRunGameplay();
}

void UActorComponent::DispatchInitializeComponent()
{
	// 등록이 끝난 컴포넌트만 초기화 가능
	if (!CanRunGameplay() || bInitialized || bInitializing)
	{
		return;
	}
	bInitializing = true;
	InitializeComponent();
	bInitialized = true;
	bInitializing = false;
}

void UActorComponent::DispatchBeginPlay()
{
	// 등록 및 초기화가 끝난 컴포넌트만 시작 가능
	if (!CanRunGameplay() || !bInitialized || bHasBegunPlay || bBeginningPlay)
	{
		return;
	}
	bBeginningPlay = true;
	BeginPlay();
	bHasBegunPlay = true;
	bBeginningPlay = false;

	// 이미 시작한 Actor에 나중에 추가된 컴포넌트도 자신의 Tick을 등록합니다.
	// 훅에서 등록을 해제했을 수 있으므로 반환 후 다시 검사합니다.
	if (CanRunGameplay()) 
	{
		PrimaryComponentTick.RegisterTickFunction(Owner->GetWorld()->GetTickTaskManager());
	}
}

void UActorComponent::DispatchEndPlay()
{
	// 시작하지 않았더라도 남은 Tick 등록은 해제
	PrimaryComponentTick.UnRegisterTickFunction();

	if (!bHasBegunPlay || bStopping)
	{
		return;
	}

	// 훅 내부 재호출이 같은 종료를 반복하지 않도록 먼저 상태를 변경
	bStopping = true;
	bHasBegunPlay = false;
	EndPlay();
	bStopping = false;
}

void UActorComponent::DispatchUninitializeComponent()
{
	// 시작 종료와 초기화가 반환된 뒤에만 초기화 자원을 해제
	if (!bInitialized || bHasBegunPlay || bStopping || bInitializing || bBeginningPlay)
	{
		return;
	}

	bStopping = true;
	bInitialized = false;
	UninitializeComponent();
	bStopping = false;
}