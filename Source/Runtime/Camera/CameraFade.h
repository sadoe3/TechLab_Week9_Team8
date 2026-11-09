#pragma once

#include "Math/Color.h"

// 페이드 상태와 시간 보간을 관리한다. 화면 합성은 Renderer가 담당한다.
class FCameraFade
{
public:
    void Start(float FromAlpha, float ToAlpha, float Duration,
        const FLinearColor& Color, bool bHoldFadeWhenFinished);
    void Stop();
    void Update(float DeltaTime);

    float GetAmount() const;
    const FLinearColor& GetColor() const;

private:
    float FadeAmount = 0.0f;
    float FadeStartAlpha = 0.0f;
    float FadeTargetAlpha = 0.0f;
    float FadeDuration = 0.0f;
    float FadeElapsedTime = 0.0f;
    FLinearColor FadeColor{0.0f, 0.0f, 0.0f, 1.0f};
    bool bEnabled = false;
    bool bHoldFadeWhenFinished = false;
};
