#pragma once

#include <cmath>
#include "UObject/Class.h"
#include "Camera/CameraTypes.h"
#include "Math/EngineMath.h"
#include "Math/RichCurve.h"


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

// 단일 축의 1차원 펄린 노이즈 설정
struct FPerlinNoiseShaker
{
    float Amplitude = 0.0f;  // 변화량 크기
    float Frequency = 15.0f; // 초당 격자 진행량
    uint32 Seed = 0;         // 기울기 선택 기준

    float Eval(float Time) const
    {
        // 시간에 따른 노이즈 좌표와 유한성 확인
        const double Position = static_cast<double>(Time) * Frequency;
        if (!std::isfinite(Position))
        {
            return 0.0f;
        }

        // 32비트 격자 주기와 셀 내부 좌표
        const double Wrapped = std::fmod(Position, 4294967296.0);
        const double CellPosition = std::floor(Wrapped);
        const uint32 Cell = static_cast<uint32>(static_cast<int64>(CellPosition));
        const float T = static_cast<float>(Wrapped - CellPosition);

        // 격자 좌표와 Seed로 결정하는 기울기
        const auto Gradient = [this](uint32 Index)
            {
                uint32 Hash = Index ^ Seed;
                Hash ^= Hash >> 16;
                Hash *= 0x7feb352du;    // 비트 섞기용 상수
                Hash ^= Hash >> 15;
                Hash *= 0x846ca68bu;    // 비트 섞기용 상수
                Hash ^= Hash >> 16;
                return (Hash & 1u) ? 1.0f : -1.0f;
            };

        // 격자 기여도의 5차 가중치 보간
        const float Left = Gradient(Cell) * T;
        const float Right = Gradient(Cell + 1u) * (T - 1.0f);
        const float Fade = T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f);
        return Amplitude * (2.0f * (Left + (Right - Left) * Fade));
    }
};

// 축별 펄린 노이즈 패턴
class UPerlinNoiseCameraShakePattern : public UCameraShakePattern
{
    DECLARE_CLASS(UPerlinNoiseCameraShakePattern, UCameraShakePattern)

public:
    FPerlinNoiseShaker X, Y, Z;
    FPerlinNoiseShaker Pitch, Yaw, Roll;
    FPerlinNoiseShaker FOV;

    // 테스트용 임시 생성자. 임의로 정해진 시드값을 사용함.
    UPerlinNoiseCameraShakePattern()
    {
        // 축별 노이즈 형태 구분
        X.Seed = 11;
        Y.Seed = 23;
        Z.Seed = 37;
        Pitch.Seed = 53;
        Yaw.Seed = 71;
        Roll.Seed = 89;
        FOV.Seed = 107;
    }

    void UpdateShakePattern(float ElapsedTime, FCameraShakePatternUpdateResult& OutResult) const override
    {
        // 현재 시간의 축별 변화량
        OutResult.Location = FVector(X.Eval(ElapsedTime), Y.Eval(ElapsedTime), Z.Eval(ElapsedTime));
        OutResult.Rotation = FRotator(Pitch.Eval(ElapsedTime), Yaw.Eval(ElapsedTime), Roll.Eval(ElapsedTime));
        OutResult.FOV = FOV.Eval(ElapsedTime);
    }
};


// 축별 커브 패턴
class UCurveCameraShakePattern : public UCameraShakePattern
{
    DECLARE_CLASS(UCurveCameraShakePattern, UCameraShakePattern)

public:
    FRichCurve  X, Y, Z;
    FRichCurve  Pitch, Yaw, Roll;
    FRichCurve  FOV;
    
    void UpdateShakePattern(float ElapsedTime, FCameraShakePatternUpdateResult& OutResult) const override
    {
        // 현재 시간의 축별 변화량
        OutResult.Location = FVector(X.Eval(ElapsedTime), Y.Eval(ElapsedTime), Z.Eval(ElapsedTime));
        OutResult.Rotation = FRotator(Pitch.Eval(ElapsedTime), Yaw.Eval(ElapsedTime), Roll.Eval(ElapsedTime));
        OutResult.FOV = FOV.Eval(ElapsedTime);
    }
};