// Todo: Script component
#include "EnginePCH.h"
#include "CollisionComponent.h"

#include "GameFramework/Actor.h"

void UCollisionComponent::OnBeginOverlap(UCollisionComponent* OtherComponent)
{
    if (OtherComponent == nullptr)
    {
        return;
    }

    // 알림을 받는 액터와 겹친 상대 액터를 구한다.
    AActor* OwnerActor = GetOwner();
    AActor* OtherActor = OtherComponent->GetOwner();

    // 서로 다른 유효한 액터 사이의 알림만 전달한다.
    if (!OwnerActor || !OtherActor || OwnerActor == OtherActor)
    {
        return;
    }

    // 기존 Actor → ScriptComponent → Lua 경로로 전달한다.
    OwnerActor->NotifyActorBeginOverlap(OtherActor);
}
