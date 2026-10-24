#pragma once

#include <sol/sol.hpp>

#include "ActorComponent.h"
#include "Core/EngineString.h"

class UScriptComponent : public UActorComponent
{
    DECLARE_CLASS(UScriptComponent, UActorComponent)

    REFLECT_START(ClassName)
        PROPERTY(ScriptPath)
        REFLECT_END()

public:
    UScriptComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;
    virtual void EndPlay() override;

    void OnBeginOverlap(AActor* otherActor);

private:
    FString ScriptPath;
    sol::environment ScriptEnvironment;

    sol::protected_function BeginPlayFunction;
    sol::protected_function TickFunction;
    sol::protected_function EndPlayFunction;
    sol::protected_function BeginOverlapFunction;

    bool bScriptReady = false;
};

