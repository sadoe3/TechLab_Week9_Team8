#pragma once

#include "Camera/CameraTypes.h"
#include "Camera/CameraFade.h"
#include "Camera/CameraLetterBox.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraShakeBase.h"
#include "UObject/WeakObjectPtr.h"


class UCameraModifier;

struct FTViewTarget
{
    TWeakObjectPtr<AActor> Target; // 비소유 참조
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

    // 즉시 선택 또는 시간 지정 전환
    void SetViewTarget(AActor* NewViewTarget, const FViewTargetTransitionParams& Params = {});
    // 현재 대상 조회
    AActor* GetViewTarget() const{ return ViewTarget.Target.Get(); }
    // Transition 도착 대상 조회
    AActor* GetPendingViewTarget() const{ return PendingViewTarget.Target.Get();}
    // Transition 취소
    void CancelViewTargetBlend();

    virtual void UpdateCamera(float DeltaTime);
    const FMinimalViewInfo& GetCameraCacheView() const { return CameraCachePrivate.POV; }

    void StartCameraFade(float FromAlpha, float ToAlpha, float Duration,
        const FLinearColor& Color, bool bHoldFadeWhenFinished);
    void StopCameraFade();
    float GetFadeAmount() const;
    const FLinearColor& GetFadeColor() const;

    // 각 띠의 목표 높이 비율로 전환한다. 목표 0은 숨김, 시간 0은 즉시 적용이다.
    void StartCameraLetterBox(float TargetHeightRatio, float Duration);
    void StopCameraLetterBox();
    float GetLetterBoxHeightRatio() const;

    // 등록된 Modifier 조회
    UCameraModifier* FindCameraModifierByClass(UClass* ModifierClass) const;

    // Shake 재생 제어
    UCameraShakeBase* StartCameraShake(UClass* ShakeClass, float Scale = 1.0f,
        ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal);
    // 프리셋 이름과 초기화 설정으로 재생
    UCameraShakeBase* StartCameraShake(FName PresetName, UClass* ShakeClass, bool bSingleInstance,
        const FCameraShakeInitializer& Initialize, float Scale = 1.0f,
        ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal);
    void StopCameraShake(UCameraShakeBase* Shake, bool bImmediately = false);
    void StopAllCameraShakes(bool bImmediately = false);

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
    FTViewTarget PendingViewTarget;
private:
    // 시점 대상과 독립적인 값 멤버로 페이드 상태를 유지한다.
    FCameraFade CameraFade;
    // ViewTarget과 독립적으로 유지하는 Letter Box 상태이다.
    FCameraLetterBox CameraLetterBox;
    bool bApplyingCameraModifiers = false;
    TArray<UCameraModifier*> PendingModifierDeletes;
    FCameraCacheEntry CameraCachePrivate;
    
    // 진행 중인 Transition 설정과 남은 시간
    FViewTargetTransitionParams BlendParams;
    float BlendTimeToGo = 0.0f;

    // 콜백 중 대상 변경 식별
    uint32 ViewTargetGeneration = 0;
};
