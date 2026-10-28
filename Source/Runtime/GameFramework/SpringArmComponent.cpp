#include "EnginePCH.h"
#include "GameFramework/SpringArmComponent.h"

FVector USpringArmComponent::GetLocationForAttachment() const
{
    // Component 자체는 시작점에 남고 모든 직계 자식에게 팔 끝을 부착 기준으로 제공한다.
    // Lag 없는 기본 계산은 조회 시 수행하므로 첫 Tick 전과 로드 직후에도 유효하다.
    const FQuat Rotation = GetRotationForAttachment().Quaternion();
    // 팔 길이·오프셋은 월드 거리이며 자식의 상대 위치·배율은 기존 부모 배율을 상속한다.
    return GetWorldLocation() + TargetOffset
        - Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)) * TargetArmLength
        + Rotation.RotateVector(SocketOffset);
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
    else if (Property.Name == "TargetOffset" || Property.Name == "SocketOffset")
    {
        MarkTransformDirty();
        UpdateOverlaps();
    }
}
