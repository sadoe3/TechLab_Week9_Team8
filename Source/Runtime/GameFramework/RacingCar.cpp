#include "EnginePCH.h"
#include "RacingCar.h"

#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

ARacingCar::ARacingCar()
{
	// Character 기본 메시 재사용
	UStaticMeshComponent* VehicleMesh = GetStaticMeshComponent();

	// Capsule과 같은 부모에 부착하여 충돌 도형의 상대 회전과 카메라를 분리한다.
	UCameraComponent* Camera = CreateDefaultSubobject<UCameraComponent>("VehicleCamera");
	Camera->SetupAttachment(VehicleMesh);
	Camera->SetRelativeLocation(FVector(-4.0f, 0.0f, 2.0f));
	Camera->SetRelativeRotation(FRotator(15.0f, 0.0f, 0.0f));

	// 기본 카메라 입력 비활성화
	Camera->PrimaryComponentTick.bCanEverTick = false;

	// 컴포넌트의 BeginPlay에서 읽을 Lua 파일을 미리 지정합니다.
	UScriptComponent* VehicleScript = CreateDefaultSubobject<UScriptComponent>("VehicleScript");
	VehicleScript->SetScriptPath("Assets/Scripts/RacingCar.lua");

}

void ARacingCar::BeginPlay()
{
	Super::BeginPlay();
	if (IsPendingDestroy()) return;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->OnComponentHit.RemoveAll(this);
		Capsule->OnComponentHit.AddRaw(this, &ARacingCar::HandleComponentHit);
	}

	// PIE와 게임 빌드에서 이 차량에 붙은 카메라를 시점으로 선택합니다.
	GetWorld()->SetViewTarget(this);
}

void ARacingCar::EndPlay()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->OnComponentHit.RemoveAll(this);
	}
	Super::EndPlay();
}

void ARacingCar::HandleComponentHit(UPrimitiveComponent* HitComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit)
{
	SetVelocity(FVector::ZeroVector);
}
