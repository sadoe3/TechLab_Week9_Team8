#include "EnginePCH.h"
#include "Camera/CameraProjection.h"
#include "Math/EngineMath.h"
#include <cassert>

FMatrix FCameraProjection::GetMatrix(const float AspectRatio) const
{
    assert(AspectRatio > 0.0f && NearClip > 0.0f && FarClip > NearClip);
    // 기존 UCameraComponent의 +X depth, row-vector, D3D [0,1] 투영.
    if (Mode == EProjectionMode::Perspective)
    {
        assert(FovDegrees > 0.0f && FovDegrees < 180.0f);
        // UE와 FOV의 의미를 맞추되 기존 수직 FOV 기반 투영식을 재사용한다.
        // 실제 출력 비율로 변환하므로 화면 크기가 바뀌어도 설정한 수평 시야각은 유지된다.
        const float VerticalFovRadians = 2.0f * std::atan(std::tan(FMath::DegreesToRadians(FovDegrees) * 0.5f) / AspectRatio);
        const float YScale = 1.0f / std::tan(VerticalFovRadians * 0.5f);
        const float XScale = YScale / AspectRatio;
        const float ZScale = FarClip / (FarClip - NearClip);
        const float ZOffset = -NearClip * FarClip / (FarClip - NearClip);
        return FMatrix(0, 0, ZScale, 1, XScale, 0, 0, 0, 0, YScale, 0, 0, 0, 0, ZOffset, 0);
    }
    assert(OrthoWidth > 0.0f);
    const float XScale = 2.0f / OrthoWidth;
    const float YScale = 2.0f / (OrthoWidth / AspectRatio);
    const float ZScale = 1.0f / (FarClip - NearClip);
    const float ZOffset = -NearClip / (FarClip - NearClip);
    return FMatrix(0, 0, ZScale, 0, XScale, 0, 0, 0, 0, YScale, 0, 0, 0, 0, ZOffset, 1);
}
