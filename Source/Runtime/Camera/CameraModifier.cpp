#include "EnginePCH.h"
#include "Camera/CameraModifier.h"
#include "Math/EngineMath.h"

void UCameraModifier::AddedToCamera(APlayerCameraManager* Camera)
{
    CameraOwner = Camera;
}

bool UCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
    // 비활성 Modifier를 건너뛰는 책임은 호출자인 Manager에 있다.
    UpdateAlpha(DeltaTime);
    if (bPendingDisable && Alpha <= 0.0f)
    {
        DisableModifier(true);
    }
    return false;
}

void UCameraModifier::EnableModifier()
{
    // 종료 도중 재활성화해도 현재 강도에서 이어지도록 Alpha는 보존한다.
    bDisabled = false;
    bPendingDisable = false;
}

void UCameraModifier::DisableModifier(bool bImmediate)
{
    if (bImmediate)
    {
        bDisabled = true;
        bPendingDisable = false;
    }
    else if (!bDisabled)
    {
        bPendingDisable = true;
    }
}

void UCameraModifier::UpdateAlpha(float DeltaTime)
{
    const float TargetAlpha = bPendingDisable ? 0.0f : 1.0f;
    const float BlendTime = bPendingDisable ? AlphaOutTime : AlphaInTime;
    if (BlendTime <= 0.0f)
    {
        Alpha = TargetAlpha;
    }
    else if (Alpha > TargetAlpha)
    {
        Alpha = FMath::Max(Alpha - DeltaTime / BlendTime, TargetAlpha);
    }
    else
    {
        Alpha = FMath::Min(Alpha + DeltaTime / BlendTime, TargetAlpha);
    }
}
