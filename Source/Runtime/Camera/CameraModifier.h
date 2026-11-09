#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

class APlayerCameraManager;
struct FMinimalViewInfo;

class UCameraModifier : public UObject
{
    DECLARE_CLASS(UCameraModifier, UObject)

public:
    virtual void AddedToCamera(APlayerCameraManager* Camera);
    // true는 이후 Modifier의 적용 중단을 뜻한다. POV 수정량에 Alpha를 반영하는 것은 파생 클래스의 책임이다.
    virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV);

    virtual void EnableModifier();
    virtual void DisableModifier(bool bImmediate = false);
    virtual bool IsDisabled() const { return bDisabled; }
    virtual bool IsPendingDisable() const { return bPendingDisable; }
    virtual void UpdateAlpha(float DeltaTime);

    uint8 Priority = 127; // 값이 작을수록 먼저 적용한다.
    bool bExclusive = false; // 등록 시 같은 Priority가 이미 있으면 이 Modifier의 등록을 거부한다.

protected:
    APlayerCameraManager* CameraOwner = nullptr; // Manager가 Modifier의 수명을 관리한다.
    float AlphaInTime = 0.0f;
    float AlphaOutTime = 0.0f;
    float Alpha = 0.0f;
    bool bDisabled = false;
    bool bPendingDisable = false;
};
