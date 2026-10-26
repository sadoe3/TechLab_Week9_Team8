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

	const char* BackgroundTexture = "Assets/UI/menu_background.png";

	// 메뉴 그림(거리 17)보다 멀리 둬야 반투명 정렬에서 먼저 그려져 뒤에 깔린다.
	constexpr float BackgroundDistance = 25.0f;
	// FOV 60에서 화면 높이는 거리의 약 1.155배다. 어떤 창 비율에서도 빈 데가 없도록
	// 그보다 넉넉히 잡고, 가로세로 비는 원본 그림(2206x1332)에 맞춘다.
	constexpr float BackgroundHeight = 35.0f;
	constexpr float BackgroundWidth = 58.0f;
}

AMainMenuHud::AMainMenuHud()
{
	PrimaryActorTick.bCanEverTick = true;

	camera = CreateDefaultSubobject<UCameraComponent>("MenuCamera");

	backgroundBillboard = CreateDefaultSubobject<UBillboardComponent>("BackgroundBillboard");
	backgroundBillboard->SetHiddenInGame(false);
	backgroundBillboard->SetTexture(BackgroundTexture);
	backgroundBillboard->SetRelativeLocation(FVector(BackgroundDistance, 0, 0));
	backgroundBillboard->SetRelativeScale3D(FVector(1, BackgroundWidth, BackgroundHeight));

	billboard = CreateDefaultSubobject<UBillboardComponent>("Billboard");
	billboard->SetHiddenInGame(false);
	billboard->SetTexture(MenuTextures[0]);
	billboard->SetRelativeLocation(FVector(17, 0, 0));
	billboard->SetRelativeScale3D(FVector(1, 17.32f, 20.55f));

	SetRootComponent(camera);
	backgroundBillboard->SetupAttachment(camera);
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
