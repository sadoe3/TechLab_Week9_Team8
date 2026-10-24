// Todo: Script component
#pragma once

#include "ActorComponent.h"

// 충돌 시스템의 알림을 소유 Actor에 전달한다.
class UCollisionComponent : public UActorComponent
{
    DECLARE_CLASS(UCollisionComponent, UActorComponent)

    REFLECT_START(ClassName)
    REFLECT_END()

public:
    // 새 겹침이 확정된 뒤 충돌 시스템이 호출한다.
    // OtherComponent는 겹치기 시작한 상대 컴포넌트다.
    void OnBeginOverlap(UCollisionComponent* OtherComponent);
};
