#include "EnginePCH.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Input/InputSystem.h"
#include <cassert>

UCameraComponent* FGameViewportClient::GetCamera() const
{
	// 선택된 액터에서 해석한 카메라 컴포넌트를 사용합니다.
	return World->GetMainCamera();
}

bool FGameViewportClient::HasCamera() const
{
	return World->GetPlayerCameraManager() && World->GetViewTarget();
}

void FGameViewportClient::SetInputEnabled(const bool bEnabled)
{
	World->SetCameraInputEnabled(bEnabled);
}

void FGameViewportClient::UpdateInput(const FViewportInput& Input, FVector2 OutputSize)
{
	// 기존 카메라 입력 허용 상태 처리를 유지한다.
	SetInputEnabled(Input.bEnabled);
	if (!Input.bEnabled) return;

	APlayerCameraManager* Manager = World->GetPlayerCameraManager();
	const FLinearColor Black{0.0f, 0.0f, 0.0f, 1.0f};

	// 테스트 입력은 키를 누른 첫 프레임에만 요청한다.
	if (FInputSystem::IsKeyPressed(EKeyCode::Num1))
	{
		// 1: 1초 동안 어두워지고 완료 후 암전을 유지한다.
		Manager->StartCameraFade(0.0f, 1.0f, 1.0f, Black, true);
	}
	else if (FInputSystem::IsKeyPressed(EKeyCode::Num2))
	{
		// 2: 암전에서 1초 동안 원래 화면으로 돌아온다.
		Manager->StartCameraFade(1.0f, 0.0f, 1.0f, Black, false);
	}
	else if (FInputSystem::IsKeyPressed(EKeyCode::Num3))
	{
		// 3: 진행 중이거나 유지 중인 효과를 즉시 해제한다.
		Manager->StopCameraFade();
	}
	else if (FInputSystem::IsKeyPressed(EKeyCode::Num4))
	{
		// 4: 현재 높이에서 1초 동안 각 띠를 화면 높이의 10%로 표시하고 유지한다.
		Manager->StartCameraLetterBox(0.1f, 1.0f);
	}
	else if (FInputSystem::IsKeyPressed(EKeyCode::Num5))
	{
		// 5: 표시 도중에도 현재 높이에서 이어서 1초 동안 띠를 숨긴다.
		Manager->StartCameraLetterBox(0.0f, 1.0f);
	}
}

FSceneView FGameViewportClient::CreateSceneView(const FVector2 OutputSize) const
{
	const APlayerCameraManager* Manager = World->GetPlayerCameraManager();
	assert(Manager && Manager->GetViewTarget());
	const FMinimalViewInfo& POV = Manager->GetCameraCacheView();
	// 기존 수평 기저 재구성은 Roll을 없앴다. 최종 POV는 카메라와 Manager가 결정하므로
	// 렌더는 Roll까지 그대로 반영한다. Trace/Modifier가 만든 기울기를 여기서 다시 제거하지 않는다.
	const FMatrix Rotation = POV.Rotation.Quaternion().ToFMatrix();
	FCameraProjection Projection{POV.ProjectionMode, POV.FOV, POV.OrthoWidth, POV.NearClipPlane, POV.FarClipPlane};
	FSceneView::ConstrainOrthographicWidth(Projection, FRect{0.0f, 0.0f, OutputSize.X, OutputSize.Y});
	FSceneView View(POV.Location, Rotation, Projection,
		FRect{0.0f, 0.0f, OutputSize.X, OutputSize.Y}, GetViewRenderMode());
	// 이번 View에 사용할 상태를 복사한다. 화면 합성은 Renderer가 수행한다.
	View.FinalPostProcessSettings = POV.PostProcessSettings;
	View.FadeAmount = Manager->GetFadeAmount();
	View.FadeColor = Manager->GetFadeColor();
	// Renderer는 Manager 대신 이번 View에 복사한 높이를 사용한다.
	View.LetterBoxHeightRatio = Manager->GetLetterBoxHeightRatio();
	return View;
}
