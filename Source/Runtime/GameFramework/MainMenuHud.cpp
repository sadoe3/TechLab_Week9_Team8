#include "EnginePCH.h"
#include "MainMenuHud.h"

#include "Component/BillboardComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"


AMainMenuHud::AMainMenuHud()
{
	camera = CreateDefaultSubobject<UCameraComponent>("MenuCamera");
	billboard = CreateDefaultSubobject<UBillboardComponent>("Billboard");
	billboard->SetHiddenInGame(false);
	billboard->SetTexture("Assets/UI/menu_0.png");
	billboard->SetRelativeLocation(FVector(10, 0, 0));
	billboard->SetRelativeScale3D(FVector(1, 17.32f, 11.55f));

	SetRootComponent(camera);
	billboard->SetupAttachment(camera);
}

void AMainMenuHud::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->SetViewTarget(this);
}
