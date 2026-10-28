#pragma once

#include "Camera/CameraTypes.h"
#include "GameFramework/Actor.h"

class UCameraModifier;

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
    ~APlayerCameraManager() override;

    UCameraModifier* AddNewCameraModifier(UClass* ModifierClass);
    // 제거 성공 후 외부 포인터는 사용하지 않는다. 적용 중에는 실제 해제만 순회 종료까지 미룬다.
    bool RemoveCameraModifier(UCameraModifier* Modifier);
    void ClearCameraModifiers();

    void SetViewTarget(AActor* NewViewTarget);
    AActor* GetViewTarget() const { return ViewTarget.Target; }
    virtual void UpdateCamera(float DeltaTime);
    const FMinimalViewInfo& GetCameraCacheView() const { return CameraCachePrivate.POV; }

    float DefaultFOV = 60.0f; // Camera Component가 없는 대상에 사용할 수평 시야각(degree).
    float DefaultAspectRatio = 16.0f / 9.0f;

protected:
    bool AddCameraModifierToList(UCameraModifier* NewModifier);
    // Manager 소유. Priority는 등록 전에 설정하며 등록 후 변경하지 않는다.
    TArray<UCameraModifier*> ModifierList;

    virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime);
    virtual void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV);
    void FillCameraCache(const FMinimalViewInfo& NewInfo);

    FTViewTarget ViewTarget;

private:
    bool bApplyingCameraModifiers = false;
    TArray<UCameraModifier*> PendingModifierDeletes;
    FCameraCacheEntry CameraCachePrivate;
};
