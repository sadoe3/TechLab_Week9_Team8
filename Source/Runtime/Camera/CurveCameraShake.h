#pragma once

#include "Camera/CameraShakeBase.h"

// 공통 Curve Shake
class UCurveCameraShake : public UCameraShakeBase
{
    DECLARE_CLASS(UCurveCameraShake, UCameraShakeBase)

public:
    UCurveCameraShake()
    {
        // 재생 인스턴스의 곡선 패턴 생성
        SetRootShakePattern(NewObject<UCurveCameraShakePattern>(this));
    }
};