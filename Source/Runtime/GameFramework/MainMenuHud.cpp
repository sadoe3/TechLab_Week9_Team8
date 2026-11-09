#include "EnginePCH.h"
#include "MainMenuHud.h"

#include "Component/BillboardComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Input/InputSystem.h"

namespace
{
	const char* MenuTextures[] =
	{
		"Assets/UI/menu_0.png",
		"Assets/UI/menu_1.png",
		"Assets/UI/menu_2.png",
	};
	constexpr int32 MenuCount = 3;
	const char* HelpTexture = "Assets/UI/menu_help.png";

	const char* RaceScenePath = "Assets/Scenes/MainScene.scene";
}

AMainMenuHud::AMainMenuHud()
{
	PrimaryActorTick.bCanEverTick = true;

	camera = CreateDefaultSubobject<UCameraComponent>("MenuCamera");
	camera->SetFieldOfView(91.5f);
	billboard = CreateDefaultSubobject<UBillboardComponent>("Billboard");
	billboard->SetHiddenInGame(false);
	billboard->SetTexture(MenuTextures[0]);
	billboard->SetRelativeLocation(FVector(17, 0, 0));
	billboard->SetRelativeScale3D(FVector(1, 17.32f, 20.55f));

	SetRootComponent(camera);
	billboard->SetupAttachment(camera);
}

void AMainMenuHud::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->SetViewTarget(this);

	SelectedIndex = 0;
	bShowingHelp = false;
	ApplyMenuTexture();
}

void AMainMenuHud::ApplyMenuTexture()
{
	if (!billboard) return;
	billboard->SetTexture(bShowingHelp ? HelpTexture : MenuTextures[SelectedIndex]);
}

void AMainMenuHud::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowingHelp)
	{
		if (FInputSystem::IsKeyPressed(EKeyCode::Enter) ||
			FInputSystem::IsKeyPressed(EKeyCode::Escape))
		{
			bShowingHelp = false;
			ApplyMenuTexture();
		}
		return;
	}

	const bool bUp = FInputSystem::IsKeyPressed(EKeyCode::Up) || FInputSystem::IsKeyPressed(EKeyCode::W);
	const bool bDown = FInputSystem::IsKeyPressed(EKeyCode::Down) || FInputSystem::IsKeyPressed(EKeyCode::S);

	// 바뀔때만 대체
	if (bUp != bDown)
	{
		const int32 Previous = SelectedIndex;
		SelectedIndex = (SelectedIndex + (bDown ? 1 : MenuCount - 1)) % MenuCount;
		if (SelectedIndex != Previous)
		{
			ApplyMenuTexture();
		}
	}

	if (!FInputSystem::IsKeyPressed(EKeyCode::Enter))
		return;

	switch (SelectedIndex)
	{
	case 0:
		if (GEngine) GEngine->RequestSceneChange(GetWorld(), RaceScenePath);
		break;
	case 1:
		bShowingHelp = true;
		ApplyMenuTexture();
		break;
	case 2:
		if (GEngine) GEngine->RequestExit(GetWorld());
		break;
	}
}
