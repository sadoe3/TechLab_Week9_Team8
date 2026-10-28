#include "EnginePCH.h"
#include "CameraComponent.h"
#include "Input/InputSystem.h"
#include "../Collision/Ray.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

// 선택된 카메라만 기본 입력을 처리하며 기존 외부 입력 관리 정책을 유지합니다.
void UCameraComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
	const AActor* Owner = GetOwner();
	const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (bExternalInputManaged || !World || World->GetMainCamera() != this)
		return;

    UpdateMovement(DeltaTime);
    UpdateRotation(DeltaTime);
    UpdateZoom(DeltaTime);
}

// 마우스 조작

// 키 입력을 카메라의 전방·우측·상방 이동으로 변환한다.
void UCameraComponent::UpdateMovement(float DeltaTime)
{
    if (!FInputSystem::IsMouseDown(EMouseButton::Right)) return;

    if (FInputSystem::IsKeyDown(EKeyCode::W)) Transform.Location += Transform.GetForward() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::S)) Transform.Location -= Transform.GetForward() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::A)) Transform.Location -= Transform.GetRight() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::D)) Transform.Location += Transform.GetRight() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::Q)) Transform.Location -= Transform.GetUp() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::E)) Transform.Location += Transform.GetUp() * MoveSpeed * DeltaTime;
}

// 우클릭 중 마우스 변화량을 Euler Pitch·Yaw에 적용한다.
void UCameraComponent::UpdateRotation(float DeltaTime)
{
    if (!FInputSystem::IsMouseDown(EMouseButton::Right)) return;

    Transform.Rotation.Pitch += FInputSystem::GetMouseDeltaY() * MouseSensitivity;
    Transform.Rotation.Yaw += FInputSystem::GetMouseDeltaX() * MouseSensitivity;
}

// 휠 입력으로 원근 이동 또는 직교 폭을 조절한다.
void UCameraComponent::UpdateZoom(float DeltaTime)
{
    int32 WheelDelta = FInputSystem::GetWheelDelta();
    if (WheelDelta == 0) return;

    Transform.Location += Transform.GetForward() * WheelSpeed * WheelDelta * DeltaTime;
}


FRay UCameraComponent::DeProjection(FVector2 MousePos, float ScreenW, float ScreenH)
{
    return DeprojectView(GetViewMatrix(), GetProjectionMatrix(), GetWorldLocation(), MousePos, {ScreenW, ScreenH}, bIsOrthogonal);
}

FRay UCameraComponent::DeprojectView(const FMatrix& View, const FMatrix& Projection, const FVector& Origin, const FVector2 MousePos, const FVector2 ViewSize, const bool bOrthographic)
{
    const float NDCX = 2.0f * MousePos.X / ViewSize.X - 1.0f;
    const float NDCY = 1.0f - 2.0f * MousePos.Y / ViewSize.Y;
    FVector4 NearPoint = FVector4(NDCX, NDCY, 0.0f, 1.0f) * Projection.Inverse();
    NearPoint /= NearPoint.W;
    // 회전만 역변환한다. 큰 월드 위치/Far 값끼리 빼서 방향 정밀도를 잃지 않는다.
    const FMatrix CameraRotation = View.GetTransposed();
    const FVector Offset = CameraRotation.TransformVector(NearPoint.XYZ);
    if (bOrthographic)
        return {Origin + Offset, CameraRotation.TransformVector(ForwardVector).Normalized()};
    return {Origin, Offset.Normalized()};
}

// 카메라 Transform의 역변환으로 View 행렬을 만든다.
FMatrix UCameraComponent::GetViewMatrix() const
{
    //return GetWorldMatrix().Inverse();

    FVector WorldLocation = GetWorldLocation();
    FRotator WorldRotation = GetWorldRotation();

    FQuat WorldQuat = WorldRotation.Quaternion();
    FMatrix RotationMatrix = WorldQuat.ToFMatrix();

    FMatrix InverseTranslation = FMatrix::MakeTranslation(WorldLocation * -1.0f);
    FMatrix InverseRotation = RotationMatrix.GetTransposed();

    return InverseTranslation * InverseRotation;
}

void UCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
    DesiredView.Location = GetWorldLocation();
    DesiredView.Rotation = GetWorldRotation();
    DesiredView.FOV = FieldOfView;
    DesiredView.AspectRatio = AspectRatio;
    DesiredView.ProjectionMode = bIsOrthogonal ? EProjectionMode::Orthographic : EProjectionMode::Perspective;
    DesiredView.OrthoWidth = OrthoWidth;
    DesiredView.NearClipPlane = NearClipPlane;
    DesiredView.FarClipPlane = FarClipPlane;
}

FCameraProjection UCameraComponent::GetProjectionSettings() const
{
    return {bIsOrthogonal ? EProjectionMode::Orthographic : EProjectionMode::Perspective, FieldOfView, OrthoWidth, NearClipPlane, FarClipPlane};
}

FMatrix UCameraComponent::GetPerspectiveMatrix() const
{
    FCameraProjection Projection = GetProjectionSettings();
    Projection.Mode = EProjectionMode::Perspective;
    return Projection.GetMatrix(AspectRatio);
}

FMatrix UCameraComponent::GetOrthogonalMatrix() const
{
    FCameraProjection Projection = GetProjectionSettings();
    Projection.Mode = EProjectionMode::Orthographic;
    return Projection.GetMatrix(AspectRatio);
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
    return GetProjectionMatrix(AspectRatio);
}

FMatrix UCameraComponent::GetProjectionMatrix(const float ViewAspectRatio) const
{
    return GetProjectionSettings().GetMatrix(ViewAspectRatio);
}

// 행 벡터 규약으로 View와 Projection을 곱한다.
FMatrix UCameraComponent::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}
