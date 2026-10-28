#include "EnginePCH.h"
#include "Camera/CameraLetterBox.h"
#include "Math/EngineMath.h"

void FCameraLetterBox::Start(float InTargetHeightRatio, float InDuration)
{
    // 도중 재요청도 현재 높이에서 이어서 시작한다.
    StartHeightRatio = HeightRatio;
    TargetHeightRatio = FMath::Clamp(InTargetHeightRatio, 0.0f, 0.5f);
    Duration = FMath::Max(InDuration, 0.0f);
    ElapsedTime = 0.0f;

    if (Duration <= 0.0f)
    {
        // 0초 요청은 다음 Update()를 기다리지 않고 반영한다.
        HeightRatio = TargetHeightRatio;
    }
}

void FCameraLetterBox::Stop()
{
    HeightRatio = 0.0f;
    // 시간 상태도 종료하여 다음 Update()가 보간을 재개하지 않게 한다.
    ElapsedTime = Duration;
}

void FCameraLetterBox::Update(float DeltaTime)
{
    // 완료된 전환과 0초 요청은 현재 높이를 유지한다.
    if (ElapsedTime >= Duration)
    {
        return;
    }

    ElapsedTime = FMath::Min(ElapsedTime + DeltaTime, Duration);
    // 이 지점에서는 Duration > 0이므로 나눗셈이 안전하다.
    const float HeightInterpolationAlpha = ElapsedTime / Duration;
    // 진행률이 0이면 시작 높이, 1이면 목표 높이가 된다.
    HeightRatio = StartHeightRatio * (1.0f - HeightInterpolationAlpha)
        + TargetHeightRatio * HeightInterpolationAlpha;
}
