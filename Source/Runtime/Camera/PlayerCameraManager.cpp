#include "EnginePCH.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraModifier.h"
#include "Camera/CameraModifier_CameraShake.h"
#include <algorithm>

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

void APlayerCameraManager::SetViewTarget(AActor* NewViewTarget)
{
    ViewTarget.Target = NewViewTarget;
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
    // Modifier에서 World.SetViewTarget이 UpdateCamera(0)를 다시 호출할 수 있다.
    // Target 변경은 유지하고 새 시점 계산은 다음 정상 갱신으로 미룬다.
    if (bApplyingCameraModifiers)
    {
        return;
    }
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