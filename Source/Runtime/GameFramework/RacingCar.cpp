#include "EnginePCH.h"
#include "RacingCar.h"

#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

#include "Component/TextRenderComponent.h"
#include "Core/GameHud.h"
#include "Asset/AssetManager.h"
#include "Render/Mesh.h"
#include "Render/Material.h"
#include "Math/EngineMath.h"

namespace
{
	constexpr float NeedleMinRoll = 120.0f;
	constexpr float NeedleMaxRoll = -120.0f;

	const FVector DialCenter(3.0f, 1.5f, -0.7f);

	// 눈금판 한 변 길이
	constexpr float DialSize = 0.9f;

	constexpr float DialHubOffset = DialSize * 0.077f;

	constexpr float DialPushBack = 1.25f;

	const FVector ScoreTextBase(3.0f, -1.5f, 0.8f);
	constexpr float ScoreTextSize = 0.3f;
}

ARacingCar::ARacingCar()
{
	// Character 기본 메시 재사용
	UStaticMeshComponent* VehicleMesh = GetStaticMeshComponent();

	// Capsule과 같은 부모에 부착하여 충돌 도형의 상대 회전과 카메라를 분리한다.
	UCameraComponent* Camera = VehicleCamera = CreateDefaultSubobject<UCameraComponent>("VehicleCamera");
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
	
	ScoreText = Cast<UTextRenderComponent>(FindComponentByName("ScoreText"));
	if (ScoreText) ScoreText->SetDrawOnTop(true);
	SpeedNeedlePivot = Cast<USceneComponent>(FindComponentByName("SpeedNeedlePivot"));
	SpeedDial = Cast<UStaticMeshComponent>(FindComponentByName("SpeedDial"));
	if (SpeedDial) SpeedDial->SetHudElement(true);
	if (UStaticMeshComponent* SpeedNeedle = Cast<UStaticMeshComponent>(FindComponentByName("SpeedNeedle")))
		SpeedNeedle->SetHudElement(true);
	VehicleCamera = Cast<UCameraComponent>(FindComponentByName("VehicleCamera"));

	BaseFieldOfView = VehicleCamera ? VehicleCamera->GetFieldOfView() : 0.0f;
	LastAppliedFieldOfView = -1.0f;
	UpdateHudForFieldOfView();

	FGameHud::OnScoreChanged.AddRaw(this, &ARacingCar::HandleScoreChanged);
	FGameHud::SetScore(0);
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

void ARacingCar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateHudForFieldOfView();
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

void ARacingCar::EndPlay()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->OnComponentHit.RemoveAll(this);
	}
	FGameHud::OnScoreChanged.RemoveAll(this);
	Super::EndPlay();
}

void ARacingCar::HandleComponentHit(UPrimitiveComponent* HitComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit)
{
	SetVelocity(FVector::ZeroVector);
}
