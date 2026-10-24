
#include "EnginePCH.h"
#include "ScriptComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Scripting/LuaRuntime.h"

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

    FLuaRuntime* Runtime = OwnerWorld->GetLuaRuntime();
    assert(Runtime != nullptr);
    
    ScriptEnvironment = Runtime->CreateEnvironment(OwnerActor);

    // 컴포넌트 전용 환경에서 Lua 파일을 실행한다.
    if (Runtime->LoadScript(ScriptPath, ScriptEnvironment) == false)
    {
        return;
    }

    // 선택적 이벤트 참조 보관
    CacheFunction("OnBeginOverlap", BeginOverlapFunction);
    CacheFunction("BeginPlay", BeginPlayFunction);
    CacheFunction("Tick", TickFunction);
    CacheFunction("EndPlay", EndPlayFunction);
    CacheFunction("OnEndOverlap", EndOverlapFunction);

    // 선택적 시작 콜백 실행
    if (BeginPlayFunction.valid() && !Runtime->Call(BeginPlayFunction, ScriptPath, "BeginPlay"))
    {
        return;
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

    // World Runtime을 통한 갱신 실행
    FLuaRuntime* Runtime = GetOwner()->GetWorld()->GetLuaRuntime();
    assert(Runtime != nullptr);

    Runtime->Call(TickFunction, ScriptPath, "Tick", DeltaTime);
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
        // 참조 해제 전 종료 콜백 실행
        FLuaRuntime* Runtime = GetOwner()->GetWorld()->GetLuaRuntime();
        assert(Runtime != nullptr);

        Runtime->Call(EndPlayFunction, ScriptPath, "EndPlay");
    }

    ResetScript();

    Super::EndPlay();
}

void UScriptComponent::OnBeginOverlap(AActor* OtherActor)
{
    assert(OtherActor != nullptr);

    if (!bScriptReady || !BeginOverlapFunction.valid())
    {
        return;
    }

    // Lua의 OnBeginOverlap(OtherActor)을 호출한다.
    FLuaRuntime* Runtime = GetOwner()->GetWorld()->GetLuaRuntime();
    assert(Runtime != nullptr);

    Runtime->Call(BeginOverlapFunction, ScriptPath, "OnBeginOverlap", OtherActor);
}

bool UScriptComponent::CacheFunction(const char* EventName, sol::protected_function& OutFunction)
{
    // 현재 환경에 직접 정의된 이벤트 조회
    sol::object FunctionObject = ScriptEnvironment.raw_get<sol::object>(EventName);

    if (!FunctionObject.is<sol::protected_function>())
    {
        return false;
    }

    // 함수 참조 보관
    OutFunction = FunctionObject.as<sol::protected_function>();
    return true;
}

void UScriptComponent::ResetScript()
{
    // 빈 객체로 교체하여 컴포넌트가 가진 Lua 참조를 해제한다.
    BeginPlayFunction = sol::protected_function();
    TickFunction = sol::protected_function();
    EndPlayFunction = sol::protected_function();
    BeginOverlapFunction = sol::protected_function();
    EndOverlapFunction = sol::protected_function();

    // Environment 해제
    ScriptEnvironment = sol::environment();
}