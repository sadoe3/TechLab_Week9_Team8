#include "EnginePCH.h"
#include "RacingCar.h"

#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

ARacingCar::ARacingCar()
{
	// 기본 Cube로 임시 차체를 만들고 차량 루트에 부착합니다.
	UStaticMeshComponent* VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>("VehicleMesh");
	VehicleMesh->SetupAttachment(GetRootComponent());
	VehicleMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.5f));

	// 차량 루트의 이동과 회전을 카메라도 함께 따라가도록 부착합니다.
	UCameraComponent* Camera = CreateDefaultSubobject<UCameraComponent>("VehicleCamera");
	Camera->SetupAttachment(GetRootComponent());

	// +X 전방, +Z 위쪽 기준으로 차량 뒤쪽 위에서 아래로 15도 바라봅니다.
	Camera->SetRelativeLocation(FVector(-4.0f, 0.0f, 2.0f));
	Camera->SetRelativeRotation(FRotator(15.0f, 0.0f, 0.0f));

	// 카메라의 키보드·마우스·휠 입력만 끕니다. 차량 추종과 화면 출력은 유지됩니다.
	Camera->PrimaryComponentTick.bCanEverTick = false;

	// 컴포넌트의 BeginPlay에서 읽을 Lua 파일을 미리 지정합니다.
	UScriptComponent* VehicleScript = CreateDefaultSubobject<UScriptComponent>("VehicleScript");
	VehicleScript->SetScriptPath("Assets/Scripts/RacingCar.lua");
}

void ARacingCar::BeginPlay()
{
	Super::BeginPlay();

	// PIE와 게임 빌드에서 이 차량에 붙은 카메라를 시점으로 선택합니다.
	GetWorld()->SetViewTarget(this);
}
