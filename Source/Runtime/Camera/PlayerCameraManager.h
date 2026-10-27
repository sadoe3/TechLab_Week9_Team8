#pragma once

#include "Camera/CameraTypes.h"
#include "GameFramework/Actor.h"

struct FTViewTarget
{
    AActor* Target = nullptr; // 비소유 참조. 대상 해제 전 소유자가 선택을 해제한다.
    FMinimalViewInfo POV;
};

struct FCameraCacheEntry
{
    FMinimalViewInfo POV;
};

class APlayerCameraManager : public AActor
{
    DECLARE_CLASS(APlayerCameraManager, AActor)

public:
    void SetViewTarget(AActor* NewViewTarget);
    AActor* GetViewTarget() const { return ViewTarget.Target; }
    virtual void UpdateCamera(float DeltaTime);
    const FMinimalViewInfo& GetCameraCacheView() const { return CameraCachePrivate.POV; }

    float DefaultFOV = 60.0f; // Camera Component가 없는 대상에 사용할 수평 시야각(degree).
    float DefaultAspectRatio = 16.0f / 9.0f;

protected:
    virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime);
    void FillCameraCache(const FMinimalViewInfo& NewInfo);

    FTViewTarget ViewTarget;

private:
    FCameraCacheEntry CameraCachePrivate;
};
