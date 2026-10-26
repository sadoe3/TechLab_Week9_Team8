#include "EnginePCH.h"
#include "RacingCar.h"

#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

#include "Component/TextRenderComponent.h"
#include "Core/GameHud.h"

void ARacingCar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!ScoreText) return;
	ScoreText->SetText(std::to_string(FGameHud::GetScore()));
}

ARacingCar::ARacingCar()
{
	// Character 기본 메시 재사용
	UStaticMeshComponent* VehicleMesh = GetStaticMeshComponent();
	VehicleMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.5f));

	// Capsule 루트에 추적 카메라 부착
	UCameraComponent* Camera = CreateDefaultSubobject<UCameraComponent>("VehicleCamera");
	Camera->SetupAttachment(GetRootComponent());
	Camera->SetRelativeLocation(FVector(-4.0f, 0.0f, 2.0f));
	Camera->SetRelativeRotation(FRotator(15.0f, 0.0f, 0.0f));

	// 기본 카메라 입력 비활성화
	Camera->PrimaryComponentTick.bCanEverTick = false;

	// 컴포넌트의 BeginPlay에서 읽을 Lua 파일을 미리 지정합니다.
	UScriptComponent* VehicleScript = CreateDefaultSubobject<UScriptComponent>("VehicleScript");
	VehicleScript->SetScriptPath("Assets/Scripts/RacingCar.lua");

	ScoreText = CreateDefaultSubobject<UTextRenderComponent>("ScoreText");
	ScoreText->SetupAttachment(Camera);
	ScoreText->SetRelativeLocation({ 3.0, -1.5, 0.8 });
	ScoreText->SetTextSize(0.3f);
}

void ARacingCar::BeginPlay()
{
	Super::BeginPlay();

	// PIE와 게임 빌드에서 이 차량에 붙은 카메라를 시점으로 선택합니다.
	GetWorld()->SetViewTarget(this);
	ScoreText = Cast<UTextRenderComponent>(FindComponentByName("ScoreText"));
}
