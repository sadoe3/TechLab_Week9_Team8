#include "EnginePCH.h"
#include "Camera/CameraFade.h"
#include "Math/EngineMath.h"

void FCameraFade::Start(float FromAlpha, float ToAlpha, float Duration,
    const FLinearColor& Color, bool bHoldFadeWhenFinished)
{
    // 새 요청은 기존 진행 상태를 교체한다.
    FadeStartAlpha = FMath::Clamp(FromAlpha, 0.0f, 1.0f);
    FadeTargetAlpha = FMath::Clamp(ToAlpha, 0.0f, 1.0f);
    FadeDuration = FMath::Max(Duration, 0.0f);
    FadeElapsedTime = 0.0f;
    FadeAmount = FadeStartAlpha;
    FadeColor = Color;
    this->bHoldFadeWhenFinished = bHoldFadeWhenFinished;
    bEnabled = true;

    // 시간을 진행하지 않고 초기 상태와 0초 요청을 반영한다.
    Update(0.0f);
}

void FCameraFade::Update(float DeltaTime)
{
    if (!bEnabled)
    {
        return;
    }

    FadeElapsedTime = FMath::Min(FadeElapsedTime + DeltaTime, FadeDuration);
    const float Progress = FadeDuration > 0.0f ? FadeElapsedTime / FadeDuration : 1.0f;

    // 진행률에 따라 시작 알파의 비중을 줄이고 목표 알파의 비중을 늘린다.
    FadeAmount = FadeStartAlpha * (1.0f - Progress) + FadeTargetAlpha * Progress;

    // 유지 옵션이 켜져 있으면 완료된 목표 알파를 계속 적용한다.
    if (Progress >= 1.0f && !bHoldFadeWhenFinished)
    {
        Stop();
    }
}

void FCameraFade::Stop()
{
    // 진행 중인 페이드와 완료 후 유지 중인 페이드를 모두 해제한다.
    bEnabled = false;
    FadeAmount = 0.0f;
}

float FCameraFade::GetAmount() const
{
    return FadeAmount;
}

const FLinearColor& FCameraFade::GetColor() const
{
    return FadeColor;
}
