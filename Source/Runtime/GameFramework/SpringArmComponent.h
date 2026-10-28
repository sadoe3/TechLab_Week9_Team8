#pragma once

#include "Component/SceneComponent.h"

class USpringArmComponent : public USceneComponent
{
    DECLARE_CLASS(USpringArmComponent, USceneComponent)

    REFLECT_START(USpringArmComponent)
        PROPERTY(TargetArmLength)
        PROPERTY(TargetOffset)
        PROPERTY(SocketOffset)
        PROPERTY(bEnableCameraLag)
        PROPERTY(CameraLagSpeed)
        PROPERTY(CameraLagMaxDistance)
        PROPERTY(bEnableCameraRotationLag)
        PROPERTY(CameraRotationLagSpeed)
        PROPERTY(bDoCollisionTest)
        PROPERTY(ProbeSize)
    REFLECT_END()

public:
    USpringArmComponent();
    void BeginPlay() override;
    void EndPlay() override;
    void TickComponent(float DeltaTime) override;
    FVector GetLocationForAttachment() const override;
    FRotator GetRotationForAttachment() const override;
    void PostEditChangeProperty(const FProperty& Property) override;

    float GetTargetArmLength() const { return TargetArmLength; }
    const FVector& GetTargetOffset() const { return TargetOffset; }
    const FVector& GetSocketOffset() const { return SocketOffset; }
    void SetTargetArmLength(float Length);
    void SetTargetOffset(const FVector& Offset);
    void SetSocketOffset(const FVector& Offset);
    bool IsCameraLagEnabled() const { return bEnableCameraLag; }
    float GetCameraLagSpeed() const { return CameraLagSpeed; }
    float GetCameraLagMaxDistance() const { return CameraLagMaxDistance; }
    void SetEnableCameraLag(bool bEnabled);
    void SetCameraLagSpeed(float Speed);
    void SetCameraLagMaxDistance(float Distance);
    bool IsCameraRotationLagEnabled() const { return bEnableCameraRotationLag; }
    float GetCameraRotationLagSpeed() const { return CameraRotationLagSpeed; }
    void SetEnableCameraRotationLag(bool bEnabled);
    void SetCameraRotationLagSpeed(float Speed);
    bool IsCollisionTestEnabled() const { return bDoCollisionTest; }
    float GetProbeSize() const { return ProbeSize; }
    void SetDoCollisionTest(bool bEnabled);
    void SetProbeSize(float Radius);

private:
    void UpdateCameraTickEnabled();
    FVector CalculateDesiredEndpoint() const;
    bool UpdateCollisionProbe();

    // 현재 엔진의 월드 단위. 차량 카메라의 기존 거리 4에 가까운 기본 길이를 사용한다.
    float TargetArmLength = 3.0f;
    FVector TargetOffset = FVector::ZeroVector; // 월드 공간의 시작점 오프셋.  // 로컬 오프셋을 주려면 단순히 relative transform을 변경하면 된다
    FVector SocketOffset = FVector::ZeroVector; // 팔 회전 기준 로컬 끝점 오프셋.
    bool bEnableCameraLag = false;
    float CameraLagSpeed = 10.0f;
    float CameraLagMaxDistance = 0.0f; // 0은 거리 제한 없음.

    // 실행 중 상태는 저장/복제하지 않는다. 첫 Tick에서 현재 시작점으로 초기화한다.
    FVector LaggedArmOrigin = FVector::ZeroVector;
    FVector PreviousArmOrigin = FVector::ZeroVector; // 보간 결과가 아닌 이전 프레임의 목표 위치.
    bool bCameraLagInitialized = false;
    bool bEnableCameraRotationLag = false;
    float CameraRotationLagSpeed = 10.0f;
    FQuat LaggedArmRotation = FQuat::Identity();
    bool bCameraRotationLagInitialized = false;
    bool bDoCollisionTest = true;
    float ProbeSize = 0.12f; // 현재 월드 단위의 구체 반지름. 부모 Scale을 적용하지 않는다.
    FVector ProbedEndpoint = FVector::ZeroVector;
    bool bProbeInitialized = false;
};
