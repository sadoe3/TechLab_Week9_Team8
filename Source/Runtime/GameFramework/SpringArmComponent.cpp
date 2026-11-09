#include "EnginePCH.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

USpringArmComponent::USpringArmComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    // 차량 Lua/Movement의 PrePhysics 이동 이후, World의 Transform/Camera 갱신 이전에 실행한다.
    PrimaryComponentTick.TickGroup = ETickingGroup::PostPhysics;
}

void USpringArmComponent::BeginPlay()
{
    Super::BeginPlay();
    UpdateCameraTickEnabled();
    bCameraRotationLagInitialized = false;
    bCameraLagInitialized = false;
    bProbeInitialized = false;
    UpdateCollisionProbe();
    MarkTransformDirty();
}

void USpringArmComponent::EndPlay()
{
    bCameraRotationLagInitialized = false;
    bCameraLagInitialized = false;
    bProbeInitialized = false;
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
        const FQuat ArmRotation = GetWorldRotation().Quaternion();
        FQuat DesiredRotation = ArmRotation;
        if (bCameraRotationLagInitialized && CameraRotationLagSpeed > 0.0f)
        {
            constexpr float MaxLagTimeStep = 1.0f / 120.0f;
            const float FrameTime = FMath::Max(DeltaTime, 0.0f);
            DesiredRotation = LaggedArmRotation;
            float RemainingTime = FrameTime;
            while (RemainingTime > 0.0f)
            {
                const float Step = FMath::Min(RemainingTime, MaxLagTimeStep);
                RemainingTime -= Step;
                // Euler 각도 대신 Quaternion의 최단 경로로 구간별 목표를 만든다.
                const FQuat StepTarget = RemainingTime > 0.0f
                    ? FQuat::Slerp(PreviousArmRotation, ArmRotation, 1.0f - RemainingTime / FrameTime)
                    : ArmRotation;
                // 도달한 목표는 그대로 사용해 정지 화면의 반복 정규화 흔들림을 막는다.
                DesiredRotation = DesiredRotation.Equals(StepTarget) ? StepTarget
                    : FQuat::Slerp(DesiredRotation, StepTarget, FMath::Clamp(Step * CameraRotationLagSpeed, 0.0f, 1.0f));
            }
        }
        bLagRotated = bCameraRotationLagInitialized
            && (DesiredRotation.X != LaggedArmRotation.X || DesiredRotation.Y != LaggedArmRotation.Y
                || DesiredRotation.Z != LaggedArmRotation.Z || DesiredRotation.W != LaggedArmRotation.W);
        LaggedArmRotation = DesiredRotation;
        PreviousArmRotation = ArmRotation;
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
            // 긴 프레임에서도 목표 이동 경로를 나눠 따라간다. 이전 목표와 Lag 결과는 서로 다른 상태다.
            constexpr float MaxLagTimeStep = 1.0f / 120.0f;
            const float FrameTime = FMath::Max(DeltaTime, 0.0f);
            DesiredLocation = LaggedArmOrigin;
            float RemainingTime = FrameTime;
            while (RemainingTime > 0.0f)
            {
                const float Step = FMath::Min(RemainingTime, MaxLagTimeStep);
                RemainingTime -= Step;
                const FVector StepTarget = RemainingTime > 0.0f
                    ? PreviousArmOrigin + (ArmOrigin - PreviousArmOrigin) * (1.0f - RemainingTime / FrameTime)
                    : ArmOrigin;
                const FVector Delta = StepTarget - DesiredLocation;
                DesiredLocation = Delta.SizeSquared() < 1.e-8f ? StepTarget
                    : DesiredLocation + Delta * FMath::Clamp(Step * CameraLagSpeed, 0.0f, 1.0f);
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
        PreviousArmOrigin = ArmOrigin;
        bCameraLagInitialized = true;
    }
    // 부모가 멈춘 뒤에도 Lag로 자식 위치가 바뀌므로 자식 Transform을 갱신한다.
    const bool bProbeMoved = UpdateCollisionProbe();
    if (bLagMoved || bLagRotated || bProbeMoved)
    {
        MarkTransformDirty();
        UpdateOverlaps();
    }
}

FVector USpringArmComponent::GetLocationForAttachment() const
{
    // Getter에서는 쿼리하지 않는다. 에디터/첫 초기화 전에는 보정 전 끝점을 제공한다.
    return bDoCollisionTest && bProbeInitialized ? ProbedEndpoint : CalculateDesiredEndpoint();
}

FVector USpringArmComponent::CalculateDesiredEndpoint() const
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
    bProbeInitialized = false;
    MarkTransformDirty();
    UpdateOverlaps();
}

void USpringArmComponent::SetTargetOffset(const FVector& Offset)
{
    TargetOffset = Offset;
    bProbeInitialized = false;
    MarkTransformDirty();
    UpdateOverlaps();
}

void USpringArmComponent::SetSocketOffset(const FVector& Offset)
{
    SocketOffset = Offset;
    bProbeInitialized = false;
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
    else if (Property.Name == "bDoCollisionTest") SetDoCollisionTest(bDoCollisionTest);
    else if (Property.Name == "ProbeSize") SetProbeSize(ProbeSize);
    else if (Property.Name == "TargetOffset" || Property.Name == "SocketOffset")
    {
        MarkTransformDirty();
        UpdateOverlaps();
    }
}

void USpringArmComponent::SetEnableCameraLag(bool bEnabled)
{
    bEnableCameraLag = bEnabled;
    UpdateCameraTickEnabled();
    bCameraLagInitialized = false;
    bProbeInitialized = false;
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
    UpdateCameraTickEnabled();
    bCameraRotationLagInitialized = false;
    bProbeInitialized = false;
    MarkTransformDirty();
    UpdateOverlaps();
}

void USpringArmComponent::SetCameraRotationLagSpeed(float Speed)
{
    CameraRotationLagSpeed = FMath::Max(0.0f, Speed);
}

void USpringArmComponent::UpdateCameraTickEnabled()
{
    const bool bEnabled = bEnableCameraLag || bEnableCameraRotationLag || bDoCollisionTest;
    // 등록 전에는 시작 설정으로, 등록 후에는 실행 상태로 반영한다.
    PrimaryComponentTick.bStartWithTickEnabled = bEnabled;
    PrimaryComponentTick.SetTickFunctionEnable(bEnabled);
}

bool USpringArmComponent::UpdateCollisionProbe()
{
    UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
    if (!bDoCollisionTest || !World || !World->IsGameWorld())
    {
        bProbeInitialized = false;
        return false;
    }

    const FVector DesiredEndpoint = CalculateDesiredEndpoint();
    FVector Result = DesiredEndpoint;
    if (TargetArmLength > 0.0f && ProbeSize > 0.0f)
    {
        const FVector ArmOrigin = GetWorldLocation() + TargetOffset;
        // 임시 쿼리 도형일 뿐 Body/Component 생성이나 BVH 등록은 하지 않는다.
        FCollisionWorldShape Sphere;
        Sphere.Type = ECollisionShapeType::Sphere;
        Sphere.Center = ArmOrigin;
        Sphere.Radius = ProbeSize;
        FCollisionQueryParams Params;
        Params.IgnoredOwner = GetOwner();
        Params.Channel = ECollisionChannel::Camera;
        TArray<FHitResult> Hits;
        const ECollisionQueryStatus Status = World->GetPhysicsScene().QuerySweepMulti(
            Sphere, DesiredEndpoint - ArmOrigin, Params, Hits);
        if (Status != ECollisionQueryStatus::Success)
        {
            // 쿼리 실패를 '장애물 없음'으로 취급하지 않는다. 시작점으로 보수적으로 당긴다.
            Result = ArmOrigin;
        }
        else
        {
            for (const FHitResult& Hit : Hits)
            {
                if (!Hit.bBlockingHit) continue;
                // Location은 접촉 시 구체 중심이다. 반지름을 다시 빼지 않는다.
                // 시작 겹침에서는 시작점으로 당기며, 별도의 침투 해소는 수행하지 않는다.
                Result = Hit.bStartPenetrating ? ArmOrigin : Hit.Location;
                break;
            }
        }
    }
    const bool bChanged = !bProbeInitialized || (Result - ProbedEndpoint).SizeSquared() > 0.0f;
    ProbedEndpoint = Result;
    bProbeInitialized = true;
    return bChanged;
}

void USpringArmComponent::SetDoCollisionTest(bool bEnabled)
{
    bDoCollisionTest = bEnabled;
    bProbeInitialized = false;
    UpdateCameraTickEnabled();
    MarkTransformDirty();
    UpdateOverlaps();
}

void USpringArmComponent::SetProbeSize(float Radius)
{
    ProbeSize = FMath::Max(0.0f, Radius);
    bProbeInitialized = false;
    MarkTransformDirty();
    UpdateOverlaps();
}
