#include "EnginePCH.h"
#include "Camera/PlayerCameraManager.h"

void APlayerCameraManager::SetViewTarget(AActor* NewViewTarget)
{
    ViewTarget.Target = NewViewTarget;
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
    // World가 이동 갱신 이후 명시적으로 호출한다. 일반 Actor Tick에는 등록하지 않는다.
    UpdateViewTarget(ViewTarget, DeltaTime);
    FillCameraCache(ViewTarget.POV);
}

void APlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
    // 매 갱신을 기본값에서 시작해 이전 대상의 투영 설정이나 효과가 남지 않게 한다.
    OutVT.POV = FMinimalViewInfo();
    OutVT.POV.FOV = DefaultFOV;
    OutVT.POV.AspectRatio = DefaultAspectRatio;
    if (OutVT.Target)
    {
        OutVT.Target->CalcCamera(DeltaTime, OutVT.POV);
    }
}

void APlayerCameraManager::FillCameraCache(const FMinimalViewInfo& NewInfo)
{
    CameraCachePrivate.POV = NewInfo;
}
