#include "EnginePCH.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
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
	return FSceneView(POV.Location, Rotation, Projection, FRect{0.0f, 0.0f, OutputSize.X, OutputSize.Y}, GetViewRenderMode());
}
