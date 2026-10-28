#pragma once

#include <cmath>
#include "UObject/Class.h"
#include "Camera/CameraTypes.h"
#include "Math/EngineMath.h"

// 시점에 더할 변화량
struct FCameraShakePatternUpdateResult
{
    FVector Location;
    FRotator Rotation;
    float FOV = 0.0f;
};

// 시간에 따른 변화량 계산
class UCameraShakePattern : public UObject
{
    DECLARE_CLASS(UCameraShakePattern, UObject)

public:
    float Duration = 0.35f; // 0 이하는 무한 재생
    float BlendInTime = 0.03f;
    float BlendOutTime = 0.12f;

    virtual void UpdateShakePattern(
        float ElapsedTime,
        FCameraShakePatternUpdateResult& OutResult) const = 0;
};

// 단일 축의 사인파 설정
struct FWaveOscillator
{
    float Amplitude = 0.0f;     // 진폭
    float Frequency = 20.0f;    // 주파수 = 주기의 역수
    float Phase = 0.0f;         // 위상

    float Eval(float Time) const
    {
        // 시간 초, 주파수 Hz, 위상 radian
        return Amplitude * std::sin(2.0f * PI * Frequency * Time + Phase);
    }
};

// 축별 사인파 패턴
class UWaveOscillatorCameraShakePattern : public UCameraShakePattern
{
    DECLARE_CLASS(UWaveOscillatorCameraShakePattern, UCameraShakePattern)

public:
    // 위치,회전, FOV값에 따른 변화량을 가짐
    FWaveOscillator X, Y, Z;
    FWaveOscillator Pitch, Yaw, Roll;
    FWaveOscillator FOV;

    void UpdateShakePattern(
        float ElapsedTime,
        FCameraShakePatternUpdateResult& OutResult) const override
    {
        // 현재 시간의 축별 변화량
        OutResult.Location = FVector(X.Eval(ElapsedTime), Y.Eval(ElapsedTime), Z.Eval(ElapsedTime));
        OutResult.Rotation = FRotator(Pitch.Eval(ElapsedTime), Yaw.Eval(ElapsedTime), Roll.Eval(ElapsedTime));
        OutResult.FOV = FOV.Eval(ElapsedTime);
    }
};