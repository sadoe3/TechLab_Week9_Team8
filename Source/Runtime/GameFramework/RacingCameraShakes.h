#pragma once

// 이런식으로 클래스를 따로 파서 관리하는데에 의문이 있음
// Blueprint 이용하는거 생각하면 Lua로 관리해야할거같은데...?
#include "Camera/CameraShakeBase.h"

// 벽 충돌용 사인파 프리셋
class UWallImpactCameraShake : public UCameraShakeBase
{
    DECLARE_CLASS(UWallImpactCameraShake, UCameraShakeBase)

public:
    UWallImpactCameraShake()
    {
        // 반복 충돌의 동일 재생 재시작
        bSingleInstance = false;

        UWaveOscillatorCameraShakePattern* Pattern = NewObject<UWaveOscillatorCameraShakePattern>(this);

        // 재생 시간과 시작·종료 감쇠
        Pattern->Duration = 0.25f;
        Pattern->BlendInTime = 0.02f;
        Pattern->BlendOutTime = 0.15f;

        // 진폭·주파수·위상
        Pattern->Y = { 0.025f, 18.0f, 0.0f };
        Pattern->Z = { 0.015f, 23.0f, 0.0f };
        Pattern->Roll = { 0.8f, 20.0f, 0.0f };

        SetRootShakePattern(Pattern);
    }
};