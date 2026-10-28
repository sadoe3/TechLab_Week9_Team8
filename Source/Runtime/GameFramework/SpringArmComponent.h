#pragma once

#include "Component/SceneComponent.h"

class USpringArmComponent : public USceneComponent
{
    DECLARE_CLASS(USpringArmComponent, USceneComponent)

    REFLECT_START(USpringArmComponent)
        PROPERTY(TargetArmLength)
        PROPERTY(TargetOffset)
        PROPERTY(SocketOffset)
    REFLECT_END()

public:
    FVector GetLocationForAttachment() const override;
    void PostEditChangeProperty(const FProperty& Property) override;

    float GetTargetArmLength() const { return TargetArmLength; }
    const FVector& GetTargetOffset() const { return TargetOffset; }
    const FVector& GetSocketOffset() const { return SocketOffset; }
    void SetTargetArmLength(float Length);
    void SetTargetOffset(const FVector& Offset);
    void SetSocketOffset(const FVector& Offset);

private:
    // 현재 엔진의 월드 단위. 차량 카메라의 기존 거리 4에 가까운 기본 길이를 사용한다.
    float TargetArmLength = 3.0f;
    FVector TargetOffset = FVector::ZeroVector; // 월드 공간의 시작점 오프셋.  // 로컬 오프셋을 주려면 단순히 relative transform을 변경하면 된다
    FVector SocketOffset = FVector::ZeroVector; // 팔 회전 기준 로컬 끝점 오프셋.
};
