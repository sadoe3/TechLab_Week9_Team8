
#include "EnginePCH.h"
#include "ScriptComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/TempLuaRuntime.h"

#include "Core/EngineLog.h"

UScriptComponent::UScriptComponent()
    : ScriptPath("") // Assets/Script/LifeCycleTest.lua
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UScriptComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* OwnerActor = GetOwner();
    assert(OwnerActor != nullptr);

    UWorld* OwnerWorld = OwnerActor->GetWorld();
    assert(OwnerWorld != nullptr);

    // 편집 중에는 Lua 환경 생성과 파일 실행을 건너뛴다.
    if (OwnerWorld->GetWorldType() == EWorldType::Editor)
    {
        return;
    }

    FTempLuaRuntime* Runtime = OwnerWorld->GetLuaRuntime();
    assert(Runtime != nullptr);
    
    ScriptEnvironment = Runtime->CreateEnvironment(OwnerActor);

    // 컴포넌트 전용 환경에서 Lua 파일을 실행한다.
    if (Runtime->LoadScript(ScriptPath, ScriptEnvironment) == false)
    {
        return;
    }

    // 이 Environment의 Lua OnOverlap 함수를 찾는다.
    {
        sol::object OverlapObject = ScriptEnvironment.raw_get<sol::object>("OnOverlap");
        
        // 함수가 정의돼 있을 때만 참조를 보관한다.
        if (OverlapObject.is<sol::protected_function>())
        {
            BeginOverlapFunction = OverlapObject.as<sol::protected_function>();
        }
    }

    // 이 Environment의 BeginPlay 값을 가져온다.
    {
        sol::object BeginPlayObject = ScriptEnvironment.raw_get<sol::object>("BeginPlay");
        //assert(BeginPlayObject.is<sol::protected_function>());

        if (BeginPlayObject.is<sol::protected_function>())
        {
            BeginPlayFunction = BeginPlayObject.as<sol::protected_function>();
        }
    }
    
    // 이 Environment에서 매 프레임 호출할 Tick 함수를 찾는다.
    {
        sol::object TickObject = ScriptEnvironment.raw_get<sol::object>("Tick");
        //assert(TickObject.is<sol::protected_function>());

        if (TickObject.is<sol::protected_function>())
        {
            TickFunction = TickObject.as<sol::protected_function>();
        }
    }
    
    // 이 Environment에서 종료 시, 호출할 EndPlay 함수를 찾는다.
    {
        sol::object EndPlayObject = ScriptEnvironment.raw_get<sol::object>("EndPlay");
        //assert(EndPlayObject.is<sol::protected_function>());

        if (EndPlayObject.is<sol::protected_function>())
        {
            EndPlayFunction = EndPlayObject.as<sol::protected_function>();
        }
    }

    // Lua 파일에 BeginPlay 함수가 있을 때만 호출한다.
    if (BeginPlayFunction.valid())
    {
        // protected_function은 Lua 오류를 결과로 반환한다.
        sol::protected_function_result Result = BeginPlayFunction();

        if (Result.valid() == false)
        {
            sol::error Error = Result;
            HTR_LOG(Error, "Lua BeginPlay failed ({}): {}", ScriptPath, Error.what());

            return;
        }
    }

    // BeginPlay가 없거나 정상 실행되었으면 갱신을 허용한다.
    bScriptReady = true;
}

void UScriptComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (bScriptReady == false || TickFunction.valid() == false)
    {
        return;
    }

    // Lua Tick(DeltaTime)을 호출하고 실행 결과를 받는다.
    sol::protected_function_result Result = TickFunction(DeltaTime);
    if (Result.valid() == false)
    {
        sol::error Error = Result;
        HTR_LOG(Error, "Lua Tick failed ({}): {}", ScriptPath, Error.what());
    }
}

void UScriptComponent::EndPlay()
{
    // 시작 준비가 완료된 스크립트만 종료 콜백을 호출한다.
    //assert(bScriptReady);
    //assert(EndPlayFunction.valid());

    const bool bCallEndPlay = bScriptReady && EndPlayFunction.valid();

    // 종료 상태로 전환하여 중복 종료 콜백 호출을 막는다.
    bScriptReady = false;
    PrimaryComponentTick.UnRegisterTickFunction();

    // 참조를 해제하기 전에 Lua EndPlay()를 실행한다.
    if (bCallEndPlay)
    {
        sol::protected_function_result Result = EndPlayFunction();
        if (Result.valid() == false)
        {
            sol::error Error = Result;
            HTR_LOG(Error, "Lua EndPlay failed ({}): {}", ScriptPath, Error.what());
        }
    }

    // 빈 객체로 교체하여 컴포넌트가 가진 Lua 참조를 해제한다.
    BeginPlayFunction = sol::protected_function();
    TickFunction = sol::protected_function();
    EndPlayFunction = sol::protected_function();
    ScriptEnvironment = sol::environment();
    BeginOverlapFunction = sol::protected_function();

    Super::EndPlay();
}

void UScriptComponent::OnBeginOverlap(AActor* OtherActor)
{
    assert(OtherActor != nullptr);

    if (bScriptReady == false || BeginOverlapFunction.valid() == false)
    {
        return;
    }

    // C++ 겹침 시작 알림을 Lua OnOverlap(OtherActor)으로 전달한다.
    sol::protected_function_result Result = BeginOverlapFunction(OtherActor);

    // Lua 오류를 엔진 로그에 기록한다.
    if (Result.valid() == false)
    {
        sol::error Error = Result;
        HTR_LOG(Error, "Lua OnOverlap failed ({}): {}", ScriptPath, Error.what());
    }
}


