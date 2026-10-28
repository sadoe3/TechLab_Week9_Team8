#pragma once

#include "Camera/CameraProjection.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Render/PostProcessSettings.h"

struct FMinimalViewInfo
{
    FVector Location = FVector();
    FRotator Rotation = FRotator();
    // UE의 시점 인터페이스와 의미를 맞추기 위해 수평 시야각(degree)을 전달한다.
    float FOV = 60.0f;
    float AspectRatio = 16.0f / 9.0f;
    EProjectionMode ProjectionMode = EProjectionMode::Perspective;
    float OrthoWidth = 16.0f;
    float NearClipPlane = 0.1f;
    float FarClipPlane = 10000.0f;

    // 위치·회전·투영 정보와 함께 보관하는 이번 POV의 후처리 설정이다.
    FPostProcessSettings PostProcessSettings;
    // 다른 시점과 지정 비율로 합성
    void BlendViewInfo(const FMinimalViewInfo& Other, float Alpha);
};

// 기본 시점 전환 보간 방식
enum class EViewTargetBlendFunction : uint8
{
    Linear,
    Cubic,
    EaseIn,
    EaseOut,
    EaseInOut
};

// 시점 전환 설정
struct FViewTargetTransitionParams
{
    float BlendTime = 0.0f; // 전환 시간, 초
    EViewTargetBlendFunction BlendFunction = EViewTargetBlendFunction::Cubic;
    float BlendExp = 2.0f; // Ease 계열의 곡선 지수
    bool bLockOutgoing = false; // 출발 시점 고정

    // 경과 시간 비율의 합성 비율 변환
    float GetBlendAlpha(float TimeAlpha) const;
};