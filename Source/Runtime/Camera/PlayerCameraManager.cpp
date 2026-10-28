#include "EnginePCH.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraModifier.h"
#include "Camera/CameraModifier_CameraShake.h"
#include <algorithm>
#include <cmath>

APlayerCameraManager::~APlayerCameraManager()
{
    ClearCameraModifiers();
}

UCameraModifier* APlayerCameraManager::AddNewCameraModifier(UClass* ModifierClass)
{
    if (!ModifierClass || !ModifierClass->IsChildOf(UCameraModifier::StaticClass()))
    {
        return nullptr;
    }

    UCameraModifier* NewModifier = NewObject<UCameraModifier>(this, ModifierClass);
    if (!NewModifier)
    {
        return nullptr;
    }
    if (!AddCameraModifierToList(NewModifier))
    {
        delete NewModifier;
        return nullptr;
    }
    NewModifier->AddedToCamera(this);
    return NewModifier;
}

bool APlayerCameraManager::AddCameraModifierToList(UCameraModifier* NewModifier)
{
    if (!NewModifier || NewModifier->GetOuter() != this)
    {
        return false;
    }

    int32 InsertIndex = ModifierList.Num();
    for (int32 Index = 0; Index < ModifierList.Num(); ++Index)
    {
        UCameraModifier* Existing = ModifierList[Index];
        if (Existing == NewModifier ||
            (NewModifier->bExclusive && Existing->Priority == NewModifier->Priority))
        {
            return false;
        }
        if (InsertIndex == ModifierList.Num() && NewModifier->Priority <= Existing->Priority)
        {
            InsertIndex = Index;
        }
    }
    ModifierList.Insert(NewModifier, InsertIndex);
    return true;
}

bool APlayerCameraManager::RemoveCameraModifier(UCameraModifier* Modifier)
{
    for (int32 Index = 0; Index < ModifierList.Num(); ++Index)
    {
        if (ModifierList[Index] == Modifier)
        {
            ModifierList.RemoveAt(Index);
            if (bApplyingCameraModifiers)
            {
                // 실행 중인 자기 자신도 정상 반환할 수 있도록 Owner와 객체를 유지한다.
                PendingModifierDeletes.Add(Modifier);
            }
            else
            {
                Modifier->AddedToCamera(nullptr);
                delete Modifier;
            }
            return true;
        }
    }
    return false;
}

void APlayerCameraManager::ClearCameraModifiers()
{
    while (!ModifierList.IsEmpty())
    {
        RemoveCameraModifier(ModifierList[ModifierList.Num() - 1]);
    }
}

void APlayerCameraManager::SetViewTarget(AActor* NewViewTarget, const FViewTargetTransitionParams& Params)
{
    // 기존 기본 보간 요청
    SetViewTargetInternal(NewViewTarget, Params, nullptr);
}

void APlayerCameraManager::SetViewTargetWithCurve(AActor* NewViewTarget, const FViewTargetTransitionParams& Params,
    const FCameraTransitionCurve& Curve)
{
    // 사용자 시간 곡선 요청
    SetViewTargetInternal(NewViewTarget, Params, &Curve);
}

void APlayerCameraManager::SetViewTargetInternal(AActor* NewViewTarget, const FViewTargetTransitionParams& Params,
    const FCameraTransitionCurve* Curve)
{
    // 삭제 예약 대상 제외
    if (NewViewTarget && NewViewTarget->IsPendingDestroy())
    {
        return;
    }

    AActor* Current = GetViewTarget();

    // 전환 중이 아닌 동일 대상 요청 유지
    if (BlendTimeToGo <= 0.0f && NewViewTarget == Current)
    {
        return;
    }

    // 즉시 선택과 진행 중 전환 취소
    if (!NewViewTarget || !Current || Current->IsPendingDestroy()
        || !std::isfinite(Params.BlendTime) || Params.BlendTime <= 0.0f)
    {
        CancelViewTargetBlend();
        ViewTarget.Target = NewViewTarget;
        return;
    }

    // 동일 도착 대상의 재시작 방지
    if (NewViewTarget == GetPendingViewTarget())
    {
        return;
    }

    // 고정 출발과 중도 재지정의 직전 화면 사용
    const bool bWasBlending = BlendTimeToGo > 0.0f;
    if (Params.bLockOutgoing || bWasBlending)
    {
        ViewTarget.POV = CameraCachePrivate.POV;
    }

    PendingViewTarget = FTViewTarget();
    PendingViewTarget.Target = NewViewTarget;
    BlendParams = Params;
    BlendParams.bLockOutgoing = Params.bLockOutgoing || bWasBlending;
    BlendTimeToGo = Params.BlendTime;

    // 수락한 전환 설정의 독립 복사
    if (Curve)
    {
        ActiveBlendCurve.Emplace(*Curve);
    }
    else
    {
        ActiveBlendCurve.Reset();
    }

    ++ViewTargetGeneration;
}
void APlayerCameraManager::CancelViewTargetBlend()
{
    // 현재 대상 유지와 전환 상태 해제
    PendingViewTarget = FTViewTarget();
    BlendParams = FViewTargetTransitionParams();
    BlendTimeToGo = 0.0f;
    ActiveBlendCurve.Reset();
    ++ViewTargetGeneration;
}

void APlayerCameraManager::StartCameraFade(float FromAlpha, float ToAlpha, float Duration,
    const FLinearColor& Color, bool bHoldFadeWhenFinished)
{
    // 초기화와 시간 보간 규칙은 FCameraFade에 위임한다.
    CameraFade.Start(FromAlpha, ToAlpha, Duration, Color, bHoldFadeWhenFinished);
}

void APlayerCameraManager::StopCameraFade()
{
    CameraFade.Stop();
}

float APlayerCameraManager::GetFadeAmount() const
{
    return CameraFade.GetAmount();
}

const FLinearColor& APlayerCameraManager::GetFadeColor() const
{
    return CameraFade.GetColor();
}

void APlayerCameraManager::StartCameraLetterBox(float TargetHeightRatio, float Duration)
{
    CameraLetterBox.Start(TargetHeightRatio, Duration);
}

void APlayerCameraManager::StopCameraLetterBox()
{
    CameraLetterBox.Stop();
}

float APlayerCameraManager::GetLetterBoxHeightRatio() const
{
    return CameraLetterBox.GetHeightRatio();
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
    // Modifier에서 World.SetViewTarget이 UpdateCamera(0)를 다시 호출할 수 있다.
    // Target 변경은 유지하고 새 시점 계산은 다음 정상 갱신으로 미룬다.
    if (bApplyingCameraModifiers)
    {
        return;
    }
    // 삭제된 도착 대상의 전환 취소
    AActor* PendingActor = GetPendingViewTarget();
    if (BlendTimeToGo > 0.0f && (!PendingActor || PendingActor->IsPendingDestroy()))
    {
        CancelViewTargetBlend();
    }

    const float Step = FMath::Max(DeltaTime, 0.0f);
    CameraFade.Update(DeltaTime);
    CameraLetterBox.Update(DeltaTime);
    const uint32 UpdatingGeneration = ViewTargetGeneration;
    FTViewTarget Current = ViewTarget;
    FTViewTarget Pending = PendingViewTarget;
    const bool bBlending = BlendTimeToGo > 0.0f;

    // 고정되지 않은 출발 시점 갱신
    if (!bBlending || !BlendParams.bLockOutgoing)
    {
        UpdateViewTarget(Current, Step);
    }
    if (ViewTargetGeneration != UpdatingGeneration)
    {
        return;
    }

    FMinimalViewInfo NewPOV = Current.POV;
    if (bBlending)
    {
        // 콜백의 삭제 예약 여부 확인
        const auto TargetsStillValid = [&Current, &Pending]()
            {
                AActor* From = Current.Target.Get();
                AActor* To = Pending.Target.Get();
                return From && !From->IsPendingDestroy() && To && !To->IsPendingDestroy();
            };
        if (!TargetsStillValid())
        {
            CancelViewTargetBlend();
            return;
        }
        // 도착 대상의 현재 시점 갱신
        UpdateViewTarget(Pending, Step);
        if (ViewTargetGeneration != UpdatingGeneration)
        {
            return;
        }
        if (!TargetsStillValid())
        {
            CancelViewTargetBlend();
            return;
        }
        BlendTimeToGo = FMath::Max(0.0f, BlendTimeToGo - Step);
        if (BlendTimeToGo <= 0.0f || Current.POV.ProjectionMode != Pending.POV.ProjectionMode)
        {
            // 완료 또는 투영 방식 변경 시 도착 시점 채택
            ViewTarget = Pending;
            NewPOV = Pending.POV;
            CancelViewTargetBlend();
        }
        else
        {
            // 선택한 시간 보간 방식으로 시점 합성
            ViewTarget = Current;
            PendingViewTarget = Pending;
            const float TimeAlpha = 1.0f - BlendTimeToGo / BlendParams.BlendTime;
            const float Alpha = ActiveBlendCurve.IsSet() ? ActiveBlendCurve->Eval(TimeAlpha) : BlendParams.GetBlendAlpha(TimeAlpha);

            NewPOV.BlendViewInfo(Pending.POV, Alpha);
        }
    }
    else
    {
        ViewTarget = Current;
    }
    FillCameraCache(NewPOV);
}

void APlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
    // 매 갱신을 기본값에서 시작해 이전 대상의 투영 설정이나 효과가 남지 않게 한다.
    OutVT.POV = FMinimalViewInfo();
    OutVT.POV.FOV = DefaultFOV;
    OutVT.POV.AspectRatio = DefaultAspectRatio;

    if (AActor* Target = OutVT.Target.Get(); Target && !Target->IsPendingDestroy())
    {
        Target->CalcCamera(DeltaTime, OutVT.POV);
    }
    // 기존 대상별 Modifier 적용
    ApplyCameraModifiers(DeltaTime, OutVT.POV);

}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
    if (bApplyingCameraModifiers)
    {
        return;
    }

    bApplyingCameraModifiers = true;
    // 새로 추가된 효과는 다음 호출부터 적용한다. 제거된 객체는 아래 정리까지 살아 있다.
    const TArray<UCameraModifier*> ModifiersToApply = ModifierList;
    for (UCameraModifier* Modifier : ModifiersToApply)
    {
        if (std::find(ModifierList.begin(), ModifierList.end(), Modifier) == ModifierList.end() || Modifier->IsDisabled())
        {
            continue;
        }
        if (Modifier->ModifyCamera(DeltaTime, InOutPOV))
        {
            break;
        }
    }

    // 조기 중단도 이 경로를 거친다. 해제 콜백 중 추가 제거도 같은 큐에서 처리한다.
    while (!PendingModifierDeletes.IsEmpty())
    {
        UCameraModifier* Modifier = PendingModifierDeletes.Last();
        PendingModifierDeletes.RemoveLast();
        Modifier->AddedToCamera(nullptr);
        delete Modifier;
    }
    bApplyingCameraModifiers = false;
}

void APlayerCameraManager::FillCameraCache(const FMinimalViewInfo& NewInfo)
{
    CameraCachePrivate.POV = NewInfo;
}

UCameraModifier* APlayerCameraManager::FindCameraModifierByClass(UClass* ModifierClass) const
{
    if (!ModifierClass)
    {
        return nullptr;
    }

    for (UCameraModifier* Modifier : ModifierList)
    {
        if (Modifier->GetClass()->IsChildOf(ModifierClass))
        {
            return Modifier;
        }
    }
    return nullptr;
}

UCameraShakeBase* APlayerCameraManager::StartCameraShake(UClass* ShakeClass, float Scale, ECameraShakePlaySpace PlaySpace)
{
    // 등록된 Shake Modifier 조회
    UCameraModifier_CameraShake* Modifier = 
        Cast<UCameraModifier_CameraShake>(FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass()));
    if (!Modifier)
    {
        Modifier = Cast<UCameraModifier_CameraShake>(AddNewCameraModifier(UCameraModifier_CameraShake::StaticClass()));
    }
    return Modifier ? Modifier->AddCameraShake(ShakeClass, Scale, PlaySpace) : nullptr;
}

UCameraShakeBase* APlayerCameraManager::StartCameraShake(FName PresetName, UClass* ShakeClass, bool bSingleInstance,
    const FCameraShakeInitializer& Initialize, float Scale, ECameraShakePlaySpace PlaySpace)
{
    // 기존 Modifier 등록 경로 재사용
    UCameraModifier_CameraShake* Modifier = 
        Cast<UCameraModifier_CameraShake>(FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass()));
    if (!Modifier)
    {
        Modifier = Cast<UCameraModifier_CameraShake>(AddNewCameraModifier(UCameraModifier_CameraShake::StaticClass()));
    }

    return Modifier ? Modifier->AddCameraShake(PresetName, ShakeClass, bSingleInstance, Initialize, Scale, PlaySpace) : nullptr;
}

void APlayerCameraManager::StopCameraShake(UCameraShakeBase* Shake, bool bImmediately)
{
    // 개별 재생 정지 요청
    UCameraModifier_CameraShake* Modifier = Cast<UCameraModifier_CameraShake>(FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass()));
    if (Modifier)
    {
        Modifier->RemoveCameraShake(Shake, bImmediately);
    }
}

void APlayerCameraManager::StopAllCameraShakes(bool bImmediately)
{
    // 전체 재생 정지 요청
    UCameraModifier_CameraShake* Modifier = Cast<UCameraModifier_CameraShake>(FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass()));
    if (Modifier)
    {
        Modifier->RemoveAllCameraShakes(bImmediately);
    }
}