#pragma once

#include "Camera/CameraShakePattern.h"
#include <functional>

class APlayerCameraManager;

// 변화량 적용 좌표계
enum class ECameraShakePlaySpace : uint8
{
    CameraLocal,
    World
};

// 패턴 재생과 시점 적용
class UCameraShakeBase : public UObject
{
    DECLARE_CLASS(UCameraShakeBase, UObject)

public:
    ~UCameraShakeBase() override
    {
        // 소유 패턴 해제
        delete RootShakePattern;
    }

    bool bSingleInstance = false;   // 같은 클래스의 Shake=를 동시에 여러개 재생할수있는지 제어

    // 이름 없는 재생은 기존 클래스 기준 사용
    FName PresetName = NAME_None;

    // 초기화할 소유 패턴 조회
    UCameraShakePattern* GetRootShakePattern() const { return RootShakePattern; }

    bool StartShake(APlayerCameraManager*, float Scale, ECameraShakePlaySpace InPlaySpace)
    {
        if (!RootShakePattern)
        {
            return false;
        }

        // 재생 상태 초기화
        ShakeScale = FMath::Max(Scale, 0.0f);
        PlaySpace = InPlaySpace;
        ElapsedTime = 0.0f;
        StopElapsedTime = 0.0f;
        StopAlpha = 0.0f;
        bStopping = false;
        bActive = true;
        ++PlaybackGeneration;
        return true;
    }

    void StopShake(bool bImmediately)
    {
        if (!bActive)
        {
            return;
        }

        if (bImmediately || RootShakePattern->BlendOutTime <= 0.0f)
        {
            bActive = false;
            return;
        }

        // 현재 강도에서 종료 구간 시작
        if (!bStopping)
        {
            StopAlpha = GetEnvelope();
            StopElapsedTime = 0.0f;
            bStopping = true;
        }
    }

    bool IsFinished() const
    {
        return !bActive;
    }

    void UpdateAndApplyCameraShake(float DeltaTime, float ModifierAlpha, FMinimalViewInfo& InOutPOV)
    {
        if (!bActive)
        {
            return;
        }

        // 재생 시간 진행
        const float Step = FMath::Max(DeltaTime, 0.0f);
        ElapsedTime += Step;
        if (bStopping)
        {
            StopElapsedTime += Step;
        }

        const bool bNaturalEnd = !bStopping
            && RootShakePattern->Duration > 0.0f
            && ElapsedTime >= RootShakePattern->Duration;
        const float Envelope = GetEnvelope();
        if (bNaturalEnd || (bStopping && Envelope <= 0.0f))
        {
            bActive = false;
            return;
        }

        // 현재 재생의 패턴 평가
        const uint32 EvaluatingGeneration = PlaybackGeneration;
        FCameraShakePatternUpdateResult Result;
        RootShakePattern->UpdateShakePattern(ElapsedTime, Result);

        // 평가 도중 종료·재시작된 결과 제외
        if (!bActive || PlaybackGeneration != EvaluatingGeneration)
        {
            return;
        }

        // 공통 강도 반영
        const float Weight = ShakeScale * ModifierAlpha * Envelope;
        const FVector LocationOffset = Result.Location * Weight;
        const FRotator RotationOffset(
            Result.Rotation.Pitch * Weight,
            Result.Rotation.Yaw * Weight,
            Result.Rotation.Roll * Weight);
        const FQuat ViewRotation = InOutPOV.Rotation.Quaternion().Normalized();
        const FQuat OffsetRotation = RotationOffset.Quaternion().Normalized();

        if (PlaySpace == ECameraShakePlaySpace::CameraLocal)
        {
            // 카메라 축 기준 변화량
            InOutPOV.Location += ViewRotation.RotateVector(LocationOffset);
            InOutPOV.Rotation = (ViewRotation * OffsetRotation).Normalized().ToFRotator();
        }
        else
        {
            // 월드 축 기준 변화량
            InOutPOV.Location += LocationOffset;
            InOutPOV.Rotation = (OffsetRotation * ViewRotation).Normalized().ToFRotator();
        }

        InOutPOV.FOV += Result.FOV * Weight;
    }

protected:
    void SetRootShakePattern(UCameraShakePattern* Pattern)
    {
        // 생성자에서 패턴 소유권 전달
        delete RootShakePattern;
        RootShakePattern = Pattern;
    }

private:
    float GetEnvelope() const
    {
        // 종료 감쇠 부분
        if (bStopping)
        {
            const float Time = RootShakePattern->BlendOutTime;
            return StopAlpha * FMath::Clamp(1.0f - StopElapsedTime / Time, 0.0f, 1.0f);
        }

        // 시작·자연 종료 강도의 최솟값
        float Weight = 1.0f;
        if (RootShakePattern->BlendInTime > 0.0f)
        {
            Weight = FMath::Clamp(ElapsedTime / RootShakePattern->BlendInTime, 0.0f, 1.0f);
        }
        if (RootShakePattern->Duration > 0.0f && RootShakePattern->BlendOutTime > 0.0f)
        {
            Weight = FMath::Min(Weight, FMath::Clamp((RootShakePattern->Duration - ElapsedTime)/ RootShakePattern->BlendOutTime, 0.0f, 1.0f));
        }
        return Weight;
    }

    // 소유 패턴과 재생 상태
    UCameraShakePattern* RootShakePattern = nullptr;
    ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal;
    float ShakeScale = 1.0f;
    float ElapsedTime = 0.0f;
    float StopElapsedTime = 0.0f;
    float StopAlpha = 0.0f;
    uint32 PlaybackGeneration = 0;
    bool bActive = false;
    bool bStopping = false;
    // 재생 시작 전 설정 대입
    using FCameraShakeInitializer = std::function<void(UCameraShakeBase&)>;
};