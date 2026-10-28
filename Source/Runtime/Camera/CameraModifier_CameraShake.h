#pragma once

#include "Camera/CameraModifier.h"
#include "Camera/CameraShakeBase.h"

// Shake 목록 관리
class UCameraModifier_CameraShake : public UCameraModifier
{
    DECLARE_CLASS(UCameraModifier_CameraShake, UCameraModifier)

public:
    UCameraModifier_CameraShake()
    {
        // 등록 우선순위와 비활성 감쇠
        Priority = 200;
        AlphaInTime = 0.0f;
        AlphaOutTime = 0.15f;
    }

    ~UCameraModifier_CameraShake() override
    {
        // 소유 재생 인스턴스 해제
        for (UCameraShakeBase* Shake : ActiveShakes)
        {
            delete Shake;
        }
    }

    UCameraShakeBase* AddCameraShake(UClass* ShakeClass, float Scale, ECameraShakePlaySpace PlaySpace)
    {
        if (!ShakeClass || !ShakeClass->IsChildOf(UCameraShakeBase::StaticClass()))
        {
            return nullptr;
        }

        // 동일 클래스의 단일 재생 재시작
        for (UCameraShakeBase* Shake : ActiveShakes)
        {
            if (Shake->PresetName.IsNone() && Shake->GetClass() == ShakeClass && Shake->bSingleInstance)
            {
                Shake->StartShake(CameraOwner, Scale, PlaySpace);
                EnableModifier();
                return Shake;
            }
        }

        // 새 재생 인스턴스 생성
        UCameraShakeBase* Shake = NewObject<UCameraShakeBase>(this, ShakeClass);
        if (!Shake || !Shake->StartShake(CameraOwner, Scale, PlaySpace))
        {
            delete Shake;
            return nullptr;
        }

        ActiveShakes.Add(Shake);
        EnableModifier();
        return Shake;
    }
    // 프리셋 이름 기준 재생 요청
    UCameraShakeBase* AddCameraShake(FName PresetName, UClass* ShakeClass, bool bSingleInstance,
        const FCameraShakeInitializer& Initialize, float Scale, ECameraShakePlaySpace PlaySpace)
    {
        if (PresetName.IsNone() || !ShakeClass || !ShakeClass->IsChildOf(UCameraShakeBase::StaticClass()))
        {
            return nullptr;
        }

        UCameraShakeBase* Reusable = nullptr;
        if (bSingleInstance)
        {
            // 같은 종류·이름의 재생을 하나로 정리
            for (UCameraShakeBase* Shake : ActiveShakes)
            {
                if (Shake->GetClass() == ShakeClass && Shake->PresetName == PresetName)
                {
                    if (!Reusable)
                    {
                        Reusable = Shake;
                    }
                    else
                    {
                        Shake->StopShake(true);
                    }
                }
            }
        }

        UCameraShakeBase* Shake = Reusable ? Reusable : NewObject<UCameraShakeBase>(this, ShakeClass);
        if (!Shake)
        {
            return nullptr;
        }

        // 재시작 전에 최신 프리셋 반영
        if (Initialize)
        {
            Initialize(*Shake);
        }
        Shake->PresetName = PresetName;
        Shake->bSingleInstance = bSingleInstance;

        if (!Shake->StartShake(CameraOwner, Scale, PlaySpace))
        {
            if (!Reusable)
            {
                delete Shake;
            }
            return nullptr;
        }

        if (!Reusable)
        {
            ActiveShakes.Add(Shake);
        }

        EnableModifier();
        if (!bUpdatingShakes)
        {
            RemoveFinishedShakes();
        }
        return Shake;
    }

    void RemoveCameraShake(UCameraShakeBase* Target, bool bImmediately)
    {
        // 소유 목록의 재생 정지
        for (UCameraShakeBase* Shake : ActiveShakes)
        {
            if (Shake == Target)
            {
                Shake->StopShake(bImmediately);
                break;
            }
        }
        if (!bUpdatingShakes)
        {
            RemoveFinishedShakes();
        }
    }

    void RemoveAllCameraShakes(bool bImmediately)
    {
        // 전체 재생 정지
        for (UCameraShakeBase* Shake : ActiveShakes)
        {
            Shake->StopShake(bImmediately);
        }
        if (!bUpdatingShakes)
        {
            RemoveFinishedShakes();
        }
    }

    void DisableModifier(bool bImmediate = false) override
    {
        // 비활성 완료 시 남은 재생 정리
        if (bImmediate)
        {
            RemoveAllCameraShakes(true);
        }
        Super::DisableModifier(bImmediate);
    }

    bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override
    {
        // 중첩 갱신 차단
        if (bUpdatingShakes)
        {
            return false;
        }

        Super::ModifyCamera(DeltaTime, InOutPOV);
        if (IsDisabled())
        {
            return false;
        }

        bUpdatingShakes = true;

        // 갱신 도중 추가된 재생의 적용 지연
        const TArray<UCameraShakeBase*> UpdatingShakes = ActiveShakes;
        for (UCameraShakeBase* Shake : UpdatingShakes)
        {
            if (!Shake->IsFinished())
            {
                Shake->UpdateAndApplyCameraShake(DeltaTime, Alpha, InOutPOV);
            }
        }

        bUpdatingShakes = false;
        RemoveFinishedShakes();
        return false;
    }

private:
    void RemoveFinishedShakes()
    {
        // 종료 인스턴스 일괄 해제
        for (int32 Index = ActiveShakes.Num() - 1; Index >= 0; --Index)
        {
            if (ActiveShakes[Index]->IsFinished())
            {
                delete ActiveShakes[Index];
                ActiveShakes.RemoveAt(Index);
            }
        }
    }

    TArray<UCameraShakeBase*> ActiveShakes;
    bool bUpdatingShakes = false;
};