#include "EnginePCH.h"
#include "Camera/CameraTypes.h"
#include "Math/Quat.h"
#include <cmath>


void FMinimalViewInfo::BlendViewInfo(const FMinimalViewInfo& Other, float Alpha)
{
    // 양 끝값과 투영 방식 전환 처리
    const float Weight = FMath::Clamp(Alpha, 0.0f, 1.0f);
    if (Weight <= 0.0f)
    {
        return;
    }
    if (Weight >= 1.0f || ProjectionMode != Other.ProjectionMode)
    {
        *this = Other;
        return;
    }

    // 위치·회전·연속 투영값 보간
    Location += (Other.Location - Location) * Weight;
    Rotation = FQuat::Slerp(Rotation.Quaternion(), Other.Rotation.Quaternion(), Weight).ToFRotator();

    FOV += (Other.FOV - FOV) * Weight;
    AspectRatio += (Other.AspectRatio - AspectRatio) * Weight;
    OrthoWidth += (Other.OrthoWidth - OrthoWidth) * Weight;
    NearClipPlane += (Other.NearClipPlane - NearClipPlane) * Weight;
    FarClipPlane += (Other.FarClipPlane - FarClipPlane) * Weight;
}

float FViewTargetTransitionParams::GetBlendAlpha(float TimeAlpha) const
{
    // 시간 범위와 정확한 시작·도착값
    const float Time = FMath::Clamp(TimeAlpha, 0.0f, 1.0f);
    if (Time <= 0.0f || Time >= 1.0f)
    {
        return Time;
    }

    // 잘못된 네이티브 지수의 선형 대체
    const float Exponent = std::isfinite(BlendExp) && BlendExp > 0.0f ? BlendExp : 1.0f;

    switch (BlendFunction)
    {
        // 3x^2-2x^3 smoothstep
    case EViewTargetBlendFunction::Cubic:
        return Time * Time * (3.0f - 2.0f * Time);
        // 지수
    case EViewTargetBlendFunction::EaseIn:
        return std::pow(Time, Exponent);
        // 제곱근
    case EViewTargetBlendFunction::EaseOut:
        return std::pow(Time, 1.0f / Exponent);
        // 절반부분에서 변환하는 지수 보간
    case EViewTargetBlendFunction::EaseInOut:
        if (Time < 0.5f)
        {
            return 0.5f * std::pow(Time * 2.0f, Exponent);
        }
        return 1.0f - 0.5f * std::pow((1.0f - Time) * 2.0f, Exponent);
        // 선형 보간
    case EViewTargetBlendFunction::Linear:
    default:
        return Time;
    }
}