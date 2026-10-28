#include "EnginePCH.h"
#include "RacingCar.h"

#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Input/InputSystem.h"

#include "Component/TextRenderComponent.h"
#include "Core/GameHud.h"
#include "Asset/AssetManager.h"
#include "Audio/SoundManager.h"
#include "Render/Mesh.h"
#include "Render/Material.h"
#include "Math/EngineMath.h"
#include "Text/TextRenderer.h"
#include <cmath>
#include <format>
#include <Core/TimeDilation.h>
#include <Cinematic/SequencePlayer.h>

namespace
{
	constexpr float NeedleMinRoll = 120.0f;
	constexpr float NeedleMaxRoll = -120.0f;

	const FVector DialCenter(3.0f, 1.5f, -0.7f);

	// 눈금판 한 변 길이
	constexpr float DialSize = 0.9f;

	constexpr float DialHubOffset = DialSize * 0.077f;

	constexpr float DialPushBack = 1.25f;

	const FVector ScoreTextBase(2.0f, -1.5f, 0.8f);
	constexpr float ScoreTextSize = 0.3f;

	const FVector CountdownTextBase(3.0f, 0.0f, 0.0f);
	constexpr float CountdownTextSize = 1.2f;

	const FVector RaceTimeTextBase(2.0f, 1.35f, 0.8f);
	constexpr float RaceTimeTextSize = 0.3f;

	FString FormatRaceTime(float Seconds)
	{
		if (Seconds < 0.0f) Seconds = 0.0f;

		const int32 TotalHundredths = static_cast<int32>(Seconds * 100.0f);
		const int32 Minutes = TotalHundredths / 6000;
		const int32 RemainSeconds = (TotalHundredths / 100) % 60;
		const int32 Hundredths = TotalHundredths % 100;

		FString Label = std::to_string(Minutes) + ":";
		if (RemainSeconds < 10) Label += "0";
		Label += std::to_string(RemainSeconds) + ".";
		if (Hundredths < 10) Label += "0";
		Label += std::to_string(Hundredths);
		return Label;
	}

	FString FormatLeaderboardTime(double Seconds)
	{
		// 0.01초 단위로 먼저 반올림하여 초·분의 자리 올림도 처리합니다.
		const int64 TotalHundredths = std::llround(Seconds * 100.0);
		const int64 Minutes = TotalHundredths / 6000;
		const int64 RemainingSeconds = (TotalHundredths / 100) % 60;
		const int64 Hundredths = TotalHundredths % 100;

		// 각 항목을 최소 두 자리로 표시합니다. 예: 01:23.45
		return std::format(
			"{:02}:{:02}.{:02}", Minutes, RemainingSeconds, Hundredths);
	}
}

ARacingCar::ARacingCar()
{
	// Character 기본 메시 재사용
	UStaticMeshComponent* VehicleMesh = GetStaticMeshComponent();

	// Capsule과 같은 부모에 부착하여 충돌 도형의 상대 회전과 카메라를 분리한다.
	UCameraComponent* Camera = VehicleCamera = CreateDefaultSubobject<UCameraComponent>("VehicleCamera");
	Camera->SetFieldOfView(91.5f);
	Camera->SetupAttachment(VehicleMesh);
	Camera->SetRelativeLocation(FVector(-4.0f, 0.0f, 2.0f));
	Camera->SetRelativeRotation(FRotator(15.0f, 0.0f, 0.0f));

	// 기본 카메라 입력 비활성화
	Camera->PrimaryComponentTick.bCanEverTick = false;

	// 컴포넌트의 BeginPlay에서 읽을 Lua 파일을 미리 지정합니다.
	UScriptComponent* VehicleScript = CreateDefaultSubobject<UScriptComponent>("VehicleScript");
	VehicleScript->SetScriptPath("Assets/Scripts/RacingCar.lua");

	ScoreText = CreateDefaultSubobject<UTextRenderComponent>("ScoreText");
	ScoreText->SetupAttachment(Camera);
	ScoreText->SetRelativeLocation(ScoreTextBase);
	ScoreText->SetTextSize(ScoreTextSize);
	ScoreText->SetDrawOnTop(true);

	CountdownText = CreateDefaultSubobject<UTextRenderComponent>("CountdownText");
	CountdownText->SetupAttachment(Camera);
	CountdownText->SetRelativeLocation(CountdownTextBase);
	CountdownText->SetTextSize(CountdownTextSize);
	CountdownText->SetDrawOnTop(true);
	CountdownText->SetText("");

	RaceTimeText = CreateDefaultSubobject<UTextRenderComponent>("RaceTimeText");
	RaceTimeText->SetupAttachment(Camera);
	RaceTimeText->SetRelativeLocation(RaceTimeTextBase);
	RaceTimeText->SetTextSize(RaceTimeTextSize);
	RaceTimeText->SetDrawOnTop(true);

	// 눈금판
	SpeedDial = CreateDefaultSubobject<UStaticMeshComponent>("SpeedDial");
	SpeedDial->SetupAttachment(Camera);
	SpeedDial->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("UIQuad"));
	SpeedDial->SetRelativeLocation((DialCenter + FVector(0.0f, 0.0f, DialHubOffset)) * DialPushBack);
	SpeedDial->SetRelativeScale3D(FVector(1.0f, DialSize * DialPushBack, DialSize * DialPushBack));
	SpeedDial->SetHudElement(true);
	SpeedDial->SetCastShadow(false);

	if (UMaterial* BaseMaterial = UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial"))
	{
		UMaterial* DialMaterial = UMaterial::CreateInstance(BaseMaterial);
		if (UTexture2D* DialTexture = UAssetManager::Get().LoadTexture("Assets/UI/speed_dial.png"))
		{
			if (DialMaterial->Textures.Num() > 0)
				DialMaterial->Textures[0] = DialTexture;
			else
				DialMaterial->Textures.Add(DialTexture);
		}
		DialMaterial->SetBlendState(EBlendState::AlphaBlend);
		SpeedDial->SetMaterial(0, DialMaterial);
	}

	SpeedNeedlePivot = CreateDefaultSubobject<USceneComponent>("SpeedNeedlePivot");
	SpeedNeedlePivot->SetupAttachment(Camera);
	SpeedNeedlePivot->SetRelativeLocation(DialCenter);

	UStaticMeshComponent* SpeedNeedle = CreateDefaultSubobject<UStaticMeshComponent>("SpeedNeedle");
	SpeedNeedle->SetupAttachment(SpeedNeedlePivot);
	SpeedNeedle->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Cube"));
	SpeedNeedle->SetRelativeLocation(FVector(0.0f, 0.0f, 0.15f));
	SpeedNeedle->SetRelativeScale3D(FVector(0.025f, 0.025f, 0.30f));
	SpeedNeedle->SetHudElement(true);
	SpeedNeedle->SetCastShadow(false);

	if (UMaterial* BaseMaterial = UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial"))
	{
		UMaterial* NeedleMaterial = UMaterial::CreateInstance(BaseMaterial);
		NeedleMaterial->SetBlendState(EBlendState::AlphaBlend);
		SpeedNeedle->SetMaterial(0, NeedleMaterial);
	}

	CreateLeaderboardHud();
}

void ARacingCar::BeginPlay()
{
	Super::BeginPlay();

	FTimeDilation::OnSlomoFinished.RemoveAll(this);
	FTimeDilation::OnSlomoFinished.AddRaw(this, &ARacingCar::HandleSlomoFinished);

	if (IsPendingDestroy()) return;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->OnComponentHit.RemoveAll(this);
		Capsule->OnComponentHit.AddRaw(this, &ARacingCar::HandleComponentHit);
	}

	// PIE와 게임 빌드에서 이 차량에 붙은 카메라를 시점으로 선택합니다.
	if (!FSequencePlayer::IsPlaying())
	{
		GetWorld()->SetViewTarget(this);
	}
	
	ScoreText = Cast<UTextRenderComponent>(FindComponentByName("ScoreText"));
	if (ScoreText) ScoreText->SetDrawOnTop(true);
	RaceTimeText = Cast<UTextRenderComponent>(FindComponentByName("RaceTimeText"));
	if (RaceTimeText) RaceTimeText->SetDrawOnTop(true);
	LastRaceTimeLabel.clear();
	FGameHud::SetRaceTime(0.0f);
	UpdateRaceTimeText();

	CountdownText = Cast<UTextRenderComponent>(FindComponentByName("CountdownText"));
	if (CountdownText)
	{
		CountdownText->SetDrawOnTop(true);
		CountdownText->SetText("");
	}
	SpeedNeedlePivot = Cast<USceneComponent>(FindComponentByName("SpeedNeedlePivot"));
	SpeedDial = Cast<UStaticMeshComponent>(FindComponentByName("SpeedDial"));
	if (SpeedDial) SpeedDial->SetHudElement(true);
	if (UStaticMeshComponent* SpeedNeedle = Cast<UStaticMeshComponent>(FindComponentByName("SpeedNeedle")))
		SpeedNeedle->SetHudElement(true);
	VehicleCamera = Cast<UCameraComponent>(FindComponentByName("VehicleCamera"));

	// 씬 로드·PIE 복제 후 현재 차량이 소유한 텍스트를 다시 연결합니다.
	LeaderboardTexts.Reset();
	const auto FindLeaderboardText = [this](const char* Name)
	{
		auto* Text =
			Cast<UTextRenderComponent>(FindComponentByName(Name));
		if (Text)
		{
			// 복제 시 복사되지 않는 표시 설정을 다시 적용합니다.
			Text->SetDrawOnTop(true);
			Text->SetVisible(false);
			LeaderboardTexts.Add(Text);
		}
		return Text;
	};

	LeaderboardTitleText = FindLeaderboardText("LeaderboardTitleText");
	BestScoreText = FindLeaderboardText("BestScoreText");
	BestFinishTimeText = FindLeaderboardText("BestFinishTimeText");
	FinishScoreText = FindLeaderboardText("FinishScoreText");
	FinishTimeText = FindLeaderboardText("FinishTimeText");
	RetryText = FindLeaderboardText("RetryText");
	MainMenuText = FindLeaderboardText("MainMenuText");
	BestRecordHeaderText = FindLeaderboardText("BestRecordHeaderText");
	CurrentRecordHeaderText = FindLeaderboardText("CurrentRecordHeaderText");
	BestScoreLabelText = FindLeaderboardText("BestScoreLabelText");
	BestFinishTimeLabelText = FindLeaderboardText("BestFinishTimeLabelText");
	FinishScoreLabelText = FindLeaderboardText("FinishScoreLabelText");
	FinishTimeLabelText = FindLeaderboardText("FinishTimeLabelText");
	ResultMenuPointerText = FindLeaderboardText("ResultMenuPointerText");
	bLeaderboardVisible = false;
	SelectedResultMenu = 0;
	bWaitingForConfirmRelease = true;
	UpdateLeaderboardLayout();

	BaseFieldOfView = 91.5f;
	LastAppliedFieldOfView = -1.0f;
	UpdateHudForFieldOfView();

	FGameHud::OnScoreChanged.AddRaw(this, &ARacingCar::HandleScoreChanged);
	FGameHud::SetScore(0);

	FGameHud::OnCountdownChanged.RemoveAll(this);
	FGameHud::OnCountdownChanged.AddRaw(this, &ARacingCar::HandleCountdownChanged);
	HandleCountdownChanged(FGameHud::GetCountdown());
}

// fov 조정
void ARacingCar::UpdateHudForFieldOfView()
{
	if (!VehicleCamera || BaseFieldOfView <= 0.0f) return;

	const float CurrentFov = VehicleCamera->GetFieldOfView();
	if (CurrentFov == LastAppliedFieldOfView) return;
	LastAppliedFieldOfView = CurrentFov;

	const float BaseTangent = tanf(FMath::DegreesToRadians(BaseFieldOfView) * 0.5f);
	if (BaseTangent <= 0.0f) return;
	const float Scale = tanf(FMath::DegreesToRadians(CurrentFov) * 0.5f) / BaseTangent;

	if (ScoreText)
	{
		ScoreText->SetRelativeLocation(
			FVector(ScoreTextBase.X, ScoreTextBase.Y * Scale, ScoreTextBase.Z * Scale));
		ScoreText->SetRelativeScale3D(FVector(Scale, Scale, Scale));
	}

	if (CountdownText)
	{
		CountdownText->SetRelativeScale3D(FVector(Scale, Scale, Scale));
	}

	if (RaceTimeText)
	{
		RaceTimeText->SetRelativeLocation(
			FVector(RaceTimeTextBase.X, RaceTimeTextBase.Y * Scale, RaceTimeTextBase.Z * Scale));
		RaceTimeText->SetRelativeScale3D(FVector(Scale, Scale, Scale));
	}

	if (SpeedDial)
	{
		const FVector DialBase = (DialCenter + FVector(0.0f, 0.0f, DialHubOffset)) * DialPushBack;
		const float DialScale = DialSize * DialPushBack * Scale;
		SpeedDial->SetRelativeLocation(
			FVector(DialBase.X, DialBase.Y * Scale, DialBase.Z * Scale));
		SpeedDial->SetRelativeScale3D(FVector(1.0f, DialScale, DialScale));
	}

	if (SpeedNeedlePivot)
	{
		SpeedNeedlePivot->SetRelativeLocation(
			FVector(DialCenter.X, DialCenter.Y * Scale, DialCenter.Z * Scale));
		// 바늘 메시는 피벗의 자식이라 피벗 크기만 바꾸면 같이 따라온다.
		SpeedNeedlePivot->SetRelativeScale3D(FVector(Scale, Scale, Scale));
	}
}

void ARacingCar::UpdateRaceTimeText()
{
	if (!RaceTimeText) return;

	FString Label = FormatRaceTime(FGameHud::GetRaceTime());
	if (Label == LastRaceTimeLabel) return;

	LastRaceTimeLabel = Label;
	RaceTimeText->SetText(Label);
}

void ARacingCar::CreateLeaderboardHud()
{
	// 공통 설정을 각 텍스트에 적용하고 카메라 앞에 배치합니다.
	const auto CreateText = [this](const char* Name, const FString& Label)
	{
		auto* Text = CreateDefaultSubobject<UTextRenderComponent>(Name);
		Text->SetupAttachment(VehicleCamera);
		Text->SetRelativeLocation(FVector(3.0f, 0.0f, 0.0f));
		Text->SetText(Label);
		// 트랙에 가려지지 않게 그리고, 처음에는 숨깁니다.
		Text->SetDrawOnTop(true);
		Text->SetVisible(false);
		LeaderboardTexts.Add(Text);
		return Text;
	};

	// 항목 이름과 숫자를 분리하여 글자 수에 관계없이 두 열을 정렬합니다.
	LeaderboardTitleText = CreateText(
		"LeaderboardTitleText", "리더보드");
	BestRecordHeaderText = CreateText("BestRecordHeaderText", "최고 기록");
	CurrentRecordHeaderText = CreateText("CurrentRecordHeaderText", "내 기록");
	BestScoreLabelText = CreateText("BestScoreLabelText", "최고 점수");
	BestFinishTimeLabelText = CreateText("BestFinishTimeLabelText", "최고 완주 시간");
	FinishScoreLabelText = CreateText("FinishScoreLabelText", "내 점수");
	FinishTimeLabelText = CreateText("FinishTimeLabelText", "내 완주 시간");
	BestScoreText = CreateText("BestScoreText", "0 점");
	BestFinishTimeText = CreateText("BestFinishTimeText", "00:00.00");
	FinishScoreText = CreateText("FinishScoreText", "0 점");
	FinishTimeText = CreateText("FinishTimeText", "00:00.00");
	RetryText = CreateText("RetryText", "다시 도전");
	MainMenuText = CreateText("MainMenuText", "메인 메뉴");
	ResultMenuPointerText = CreateText("ResultMenuPointerText", ">");
}

void ARacingCar::UpdateLeaderboardText()
{
	// 완주 순간 Lua에서 C++로 전달한 결과를 읽습니다.
	const FRaceResult& Result = FGameHud::GetRaceResult();
	BestScoreText->SetText(
		std::format("{} 점", Result.BestScore));
	FinishScoreText->SetText(
		std::format("{} 점", Result.FinishScore));
	BestFinishTimeText->SetText(
		FormatLeaderboardTime(Result.BestFinishTime));
	FinishTimeText->SetText(
		FormatLeaderboardTime(Result.FinishTime));
	// 숫자의 자릿수가 바뀌어도 오른쪽 끝을 같은 위치에 맞춥니다.
	UpdateLeaderboardLayout();
}

void ARacingCar::UpdateLeaderboardLayout()
{
	const auto PlaceText = [](
		UTextRenderComponent* Text, float Horizontal, float Baseline,
		float TextSize, bool bRightAligned)
	{
		if (!Text || !Text->GetFont()) return;
		Text->SetTextSize(TextSize);

		float MinY, MaxY, MinZ, MaxZ;
		if (!FTextRenderer::ComputeTextBounds(
			Text->GetText(), TextSize, *Text->GetFont(),
			MinY, MaxY, MinZ, MaxZ)) return;

		// 가운데 정렬된 글자 메시의 실제 끝점을 기준선에 맞춥니다.
		const float Edge = bRightAligned ? MaxY : MinY;
		Text->SetRelativeLocation(
			FVector(3.0f, Horizontal - Edge, Baseline));
	};

	const float LeftColumn = -2.65f;
	const float RightColumn = 1.3f;
	// 같은 행은 같은 높이, 같은 열은 같은 왼쪽·오른쪽 끝점을 사용합니다.
	PlaceText(LeaderboardTitleText, LeftColumn, 1.13f, 0.46f, false);
	PlaceText(BestRecordHeaderText, LeftColumn, 0.68f, 0.14f, false);
	PlaceText(BestScoreLabelText, LeftColumn, 0.34f, 0.18f, false);
	PlaceText(BestScoreText, RightColumn, 0.34f, 0.22f, true);
	PlaceText(BestFinishTimeLabelText, LeftColumn, 0.08f, 0.18f, false);
	PlaceText(BestFinishTimeText, RightColumn, 0.08f, 0.22f, true);
	PlaceText(CurrentRecordHeaderText, LeftColumn, -0.26f, 0.14f, false);
	PlaceText(FinishScoreLabelText, LeftColumn, -0.63f, 0.18f, false);
	PlaceText(FinishScoreText, RightColumn, -0.63f, 0.22f, true);
	PlaceText(FinishTimeLabelText, LeftColumn, -0.90f, 0.18f, false);
	PlaceText(FinishTimeText, RightColumn, -0.90f, 0.22f, true);
	PlaceText(RetryText, -2.5f, -1.32f, 0.19f, false);
	PlaceText(MainMenuText, -1.25f, -1.32f, 0.19f, false);

	// 선택 표시는 별도로 움직여 메뉴 글자의 위치가 바뀌지 않게 합니다.
	const float PointerHorizontal = SelectedResultMenu == 0 ? -2.75f : -1.5f;
	PlaceText(ResultMenuPointerText, PointerHorizontal, -1.32f, 0.19f, false);
}

void ARacingCar::UpdateLeaderboardHud()
{
	const ERaceResultScreen ResultScreen = FGameHud::GetResultScreen();
	// 완주 UI와 리더보드가 표시되는 동안 주행 HUD를 숨깁니다.
	const bool bShowDrivingHud = ResultScreen == ERaceResultScreen::Hidden;
	auto* SpeedNeedle =
		Cast<UStaticMeshComponent>(FindComponentByName("SpeedNeedle"));
	UPrimitiveComponent* DrivingHudComponents[] =
	{
		ScoreText, CountdownText, RaceTimeText, SpeedDial, SpeedNeedle
	};
	for (UPrimitiveComponent* Component : DrivingHudComponents)
	{
		if (Component && Component->IsVisible() != bShowDrivingHud)
		{
			Component->SetVisible(bShowDrivingHud);
		}
	}

	const bool bShouldShow = ResultScreen == ERaceResultScreen::Leaderboard;
	if (bShouldShow == bLeaderboardVisible) return;

	if (bShouldShow)
	{
		// 화면을 켜기 전에 기록과 초기 메뉴 선택을 준비합니다.
		SelectedResultMenu = 0;
		UpdateLeaderboardText();
		bWaitingForConfirmRelease = true;

		// 리더보드가 처음 표시될 때 기존 BGM을 결과 화면 음악으로 교체합니다.
		// 위의 표시 상태 비교 덕분에 매 프레임 다시 재생하지 않습니다.
		if (GEngine)
		{
			GEngine->GetSoundManager()->PlayBGM(
				"Assets/Audio/BGM/EndScreen.mp3", 0.7f);
		}
	}

	// 제목·기록·메뉴를 하나의 화면처럼 함께 켜고 끕니다.
	for (UTextRenderComponent* Text : LeaderboardTexts)
	{
		Text->SetVisible(bShouldShow);
	}
	bLeaderboardVisible = bShouldShow;
}

void ARacingCar::HandleLeaderboardInput()
{
	if (!bLeaderboardVisible) return;

	// 완주 UI에서 사용한 Enter를 놓은 뒤 새 입력을 받습니다.
	if (bWaitingForConfirmRelease)
	{
		bWaitingForConfirmRelease =
			FInputSystem::IsKeyDown(EKeyCode::Enter);
		return;
	}

	const bool bUp = FInputSystem::IsKeyPressed(EKeyCode::W)
		|| FInputSystem::IsKeyPressed(EKeyCode::Up);
	const bool bDown = FInputSystem::IsKeyPressed(EKeyCode::S)
		|| FInputSystem::IsKeyPressed(EKeyCode::Down);
	// 두 메뉴 사이를 이동하며, 위아래 동시 입력은 무시합니다.
	if (bUp != bDown)
	{
		SelectedResultMenu = 1 - SelectedResultMenu;
		UpdateLeaderboardLayout();
	}

	if (!FInputSystem::IsKeyPressed(EKeyCode::Enter)) return;
	if (!GEngine) return;

	// 다시 도전은 차량·코인·체크포인트를 초기화하도록 씬을 다시 읽습니다.
	const char* ScenePath = "Assets/Scenes/MainScene.scene";
	if (SelectedResultMenu == 1)
	{
		ScenePath = "Assets/Scenes/MainMenu.scene";
	}
	GEngine->RequestSceneChange(GetWorld(), ScenePath);
}

void ARacingCar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateHudForFieldOfView();
	UpdateRaceTimeText();
	// 상태를 먼저 갱신한 뒤 리더보드 메뉴 입력을 처리합니다.
	UpdateLeaderboardHud();
	HandleLeaderboardInput();
	if (!SpeedNeedlePivot) return;

	const float Limit = MaxSpeed > 0.0f ? MaxSpeed : 1.0f;
	float Ratio = GetVelocity().Length() / Limit;
	if (Ratio < 0.0f) Ratio = 0.0f;
	if (Ratio > 1.0f) Ratio = 1.0f;

	const float Roll = NeedleMinRoll + (NeedleMaxRoll - NeedleMinRoll) * Ratio;
	SpeedNeedlePivot->SetRelativeRotation(FRotator(0.0f, 0.0f, Roll));
}

void ARacingCar::HandleScoreChanged(int32 NewScore)
{
	ScoreText->SetText(std::to_string(NewScore));
}

void ARacingCar::HandleCountdownChanged(const FString& NewCountdown)
{
	if (CountdownText) CountdownText->SetText(NewCountdown);
}

void ARacingCar::EndPlay()
{
	FTimeDilation::OnSlomoFinished.RemoveAll(this);
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->OnComponentHit.RemoveAll(this);
	}
	FGameHud::OnScoreChanged.RemoveAll(this);
	FGameHud::OnCountdownChanged.RemoveAll(this);
	Super::EndPlay();
}

void ARacingCar::HandleComponentHit(UPrimitiveComponent* HitComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit)
{
	SetVelocity(FVector::ZeroVector);
}

void ARacingCar::HandleSlomoFinished()
{
	FGameHud::NotifyFinishUIClosed();
}
