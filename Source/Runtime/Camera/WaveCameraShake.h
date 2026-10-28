#pragma once

#include "Camera/CameraShakeBase.h"

// Lua 설정을 사용하는 공통 사인 Shake
class UWaveCameraShake : public UCameraShakeBase
{
    DECLARE_CLASS(UWaveCameraShake, UCameraShakeBase)

public:
    UWaveCameraShake()
    {
        // 재생 인스턴스의 사인 패턴 생성
        SetRootShakePattern(NewObject<UWaveOscillatorCameraShakePattern>(this));
    }
};