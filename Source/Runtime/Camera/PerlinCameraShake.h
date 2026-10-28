#pragma once

#include "Camera/CameraShakeBase.h"

// 공통 펄린 노이즈 Shake
class UPerlinCameraShake : public UCameraShakeBase
{
    DECLARE_CLASS(UPerlinCameraShake, UCameraShakeBase)

public:
    UPerlinCameraShake()
    {
        // 재생 인스턴스의 펄린 패턴 생성
        SetRootShakePattern(NewObject<UPerlinNoiseCameraShakePattern>(this));
    }
};