#include "EnginePCH.h"
#include "GameFramework/SpringArmComponent.h"

USpringArmComponent::USpringArmComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    // 차량 Lua/Movement의 PrePhysics 이동 이후, World의 Transform/Camera 갱신 이전에 실행한다.
    PrimaryComponentTick.TickGroup = ETickingGroup::PostPhysics;
}

void USpringArmComponent::BeginPlay()
{
    Super::BeginPlay();
    UpdateLagTickEnabled();
    bCameraRotationLagInitialized = false;
    bCameraLagInitialized = false;
}

void USpringArmComponent::EndPlay()
{
    bCameraRotationLagInitialized = false;
    bCameraLagInitialized = false;
    MarkTransformDirty();
    Super::EndPlay();
}

void USpringArmComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    bool bLagRotated = false;
    if (bEnableCameraRotationLag)
    {
        // Rotation Lag
        FQuat DesiredRotation = GetWorldRotation().Quaternion();
        if (bCameraRotationLagInitialized && CameraRotationLagSpeed > 0.0f)
        {
            DesiredRotation = FQuat::Slerp(LaggedArmRotation, DesiredRotation, FMath::Clamp(DeltaTime * CameraRotationLagSpeed, 0.0f, 1.0f));
        }
        bLagRotated = bCameraRotationLagInitialized
            && (DesiredRotation.X != LaggedArmRotation.X || DesiredRotation.Y != LaggedArmRotation.Y
                || DesiredRotation.Z != LaggedArmRotation.Z || DesiredRotation.W != LaggedArmRotation.W);
        LaggedArmRotation = DesiredRotation;
        bCameraRotationLagInitialized = true;
    }

    bool bLagMoved = false;
    if (bEnableCameraLag)
    {
        // Location Lag
        const FVector ArmOrigin = GetWorldLocation() + TargetOffset;
        FVector DesiredLocation = ArmOrigin;
        if (bCameraLagInitialized && CameraLagSpeed > 0.0f)
        {
            // UE VInterpTo와 동일하게 남은 거리의 일부를 따라간다. Getter에서는 상태를 갱신하지 않는다.
            const FVector Delta = ArmOrigin - LaggedArmOrigin;
            if (Delta.SizeSquared() >= 1.e-8f)
            {
                DesiredLocation = LaggedArmOrigin + Delta * FMath::Clamp(DeltaTime * CameraLagSpeed, 0.0f, 1.0f);
            }
        }
        if (CameraLagMaxDistance > 0.0f)
        {
            const FVector FromOrigin = DesiredLocation - ArmOrigin;
            const float DistanceSquared = FromOrigin.SizeSquared();
            if (DistanceSquared > CameraLagMaxDistance * CameraLagMaxDistance)
            {
                // 실제 거리 제한이 필요한 경우에만 제곱근을 계산한다.
                DesiredLocation = ArmOrigin + FromOrigin * (CameraLagMaxDistance / std::sqrt(DistanceSquared));
            }
        }
        bLagMoved = bCameraLagInitialized
            && (DesiredLocation - LaggedArmOrigin).SizeSquared() > 0.0f;
        LaggedArmOrigin = DesiredLocation;
        bCameraLagInitialized = true;
    }
    // 부모가 멈춘 뒤에도 Lag로 자식 위치가 바뀌므로 자식 Transform을 갱신한다.
    if (bLagMoved || bLagRotated)
    {
        MarkTransformDirty();
        UpdateOverlaps();
    }
}

FVector USpringArmComponent::GetLocationForAttachment() const
{
    // Component 자체는 시작점에 남고 모든 직계 자식에게 팔 끝을 부착 기준으로 제공한다.
    // 첫 Tick 전/로드 직후에는 현재 시작점을 사용하고, 이후 위치 Lag 결과만 읽는다.
    const FQuat Rotation = bEnableCameraRotationLag && bCameraRotationLagInitialized
        ? LaggedArmRotation : GetRotationForAttachment().Quaternion();
    // 팔 길이·오프셋은 월드 거리이며 자식의 상대 위치·배율은 기존 부모 배율을 상속한다.
    const FVector Origin = bEnableCameraLag && bCameraLagInitialized
        ? LaggedArmOrigin : GetWorldLocation() + TargetOffset;
    return Origin
        - Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)) * TargetArmLength
        + Rotation.RotateVector(SocketOffset);
}

FRotator USpringArmComponent::GetRotationForAttachment() const
{
    return bEnableCameraRotationLag && bCameraRotationLagInitialized
        ? LaggedArmRotation.ToFRotator() : GetWorldRotation();
}

void USpringArmComponent::SetTargetArmLength(float Length)
{
    TargetArmLength = FMath::Max(0.0f, Length);
    MarkTransformDirty();
    UpdateOverlaps();
}

void USpringArmComponent::SetTargetOffset(const FVector& Offset)
{
    TargetOffset = Offset;
    MarkTransformDirty();
    UpdateOverlaps();
}

void USpringArmComponent::SetSocketOffset(const FVector& Offset)
{
    SocketOffset = Offset;
    MarkTransformDirty();
    UpdateOverlaps();
}

void USpringArmComponent::PostEditChangeProperty(const FProperty& Property)
{
    Super::PostEditChangeProperty(Property);
    if (Property.Name == "TargetArmLength") SetTargetArmLength(TargetArmLength);
    else if (Property.Name == "bEnableCameraLag") SetEnableCameraLag(bEnableCameraLag);
    else if (Property.Name == "CameraLagSpeed") SetCameraLagSpeed(CameraLagSpeed);
    else if (Property.Name == "CameraLagMaxDistance") SetCameraLagMaxDistance(CameraLagMaxDistance);
    else if (Property.Name == "bEnableCameraRotationLag") SetEnableCameraRotationLag(bEnableCameraRotationLag);
    else if (Property.Name == "CameraRotationLagSpeed") SetCameraRotationLagSpeed(CameraRotationLagSpeed);
    else if (Property.Name == "TargetOffset" || Property.Name == "SocketOffset")
    {
        MarkTransformDirty();
        UpdateOverlaps();
    }
}

void USpringArmComponent::SetEnableCameraLag(bool bEnabled)
{
    bEnableCameraLag = bEnabled;
    UpdateLagTickEnabled();
    bCameraLagInitialized = false;
    MarkTransformDirty();
    UpdateOverlaps();
}

void USpringArmComponent::SetCameraLagSpeed(float Speed)
{
    CameraLagSpeed = FMath::Max(0.0f, Speed);
}

void USpringArmComponent::SetCameraLagMaxDistance(float Distance)
{
    CameraLagMaxDistance = FMath::Max(0.0f, Distance);
}

void USpringArmComponent::SetEnableCameraRotationLag(bool bEnabled)
{
    bEnableCameraRotationLag = bEnabled;
    UpdateLagTickEnabled();
    bCameraRotationLagInitialized = false;
    MarkTransformDirty();
    UpdateOverlaps();
}

void USpringArmComponent::SetCameraRotationLagSpeed(float Speed)
{
    CameraRotationLagSpeed = FMath::Max(0.0f, Speed);
}

void USpringArmComponent::UpdateLagTickEnabled()
{
    const bool bEnabled = bEnableCameraLag || bEnableCameraRotationLag;
    // 등록 전에는 시작 설정으로, 등록 후에는 실행 상태로 반영한다.
    PrimaryComponentTick.bStartWithTickEnabled = bEnabled;
    PrimaryComponentTick.SetTickFunctionEnable(bEnabled);
}