
#include "EnginePCH.h"
#include "ScriptComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/TempLuaRuntime.h"

#include "Core/EngineLog.h"

UScriptComponent::UScriptComponent()
    : ScriptPath("Assets/Scripts/HotReloadTest.lua") // Assets/Script/LifeCycleTest.lua
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

    // 편집 중에는 Lua 실행과 파일 변경 검사를 시작하지 않는다.
    if (OwnerWorld->GetWorldType() == EWorldType::Editor)
    {
        return;
    }

    // 최초 검사에서는 수정 시각 비교 없이 로드를 시도한다.
    bHasAttemptedWriteTime = false;
    bCheckScriptChanges = true;

    // 검사 간격을 미리 채워 첫 로드를 즉시 수행한다.
    TimeSinceLastScriptCheck = 0.5f;
    CheckScriptChanges(0.0f);
}

void UScriptComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    // 최초 로드에 실패했거나 Tick 함수가 없어도 변경을 확인한다.
    CheckScriptChanges(DeltaTime);

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
    const bool bCallEndPlay = bScriptReady && EndPlayFunction.valid();

    // 종료 중 재로드나 다른 Lua 콜백이 실행되지 않도록 막는다.
    bCheckScriptChanges = false;
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
    BeginOverlapFunction = sol::protected_function();
    ScriptEnvironment = sol::environment();

    // 다음 플레이에서는 파일 변경 검사도 처음부터 시작한다.
    bHasAttemptedWriteTime = false;
    TimeSinceLastScriptCheck = 0.0f;

    Super::EndPlay();
}

sol::protected_function UScriptComponent::FindScriptFunction(
    const sol::environment& Environment,
    const char* FunctionName)
{
    // 전역 환경을 조회하지 않고, 이 환경의 값만 가져온다.
    sol::object FunctionObject = Environment.raw_get<sol::object>(FunctionName);

    if (FunctionObject.is<sol::protected_function>())
    {
        return FunctionObject.as<sol::protected_function>();
    }

    // 삭제되었거나 정의되지 않은 콜백은 빈 참조로 처리한다.
    return sol::protected_function();
}

bool UScriptComponent::TryLoadScript()
{
    AActor* OwnerActor = GetOwner();
    assert(OwnerActor != nullptr);

    UWorld* OwnerWorld = OwnerActor->GetWorld();
    assert(OwnerWorld != nullptr);

    FTempLuaRuntime* Runtime = OwnerWorld->GetLuaRuntime();
    assert(Runtime != nullptr);

    // 기존 실행 환경을 수정하지 않고 교체 후보를 만든다.
    sol::environment NewEnvironment = Runtime->CreateEnvironment(OwnerActor);

    // 로드 실패 시 지역 변수만 해제되고 기존 실행은 유지된다.
    if (Runtime->LoadScript(ScriptPath, NewEnvironment) == false)
    {
        return false;
    }

    // 없는 콜백도 빈 참조로 준비하여 삭제한 이전 함수가 남지 않게 한다.
    sol::protected_function NewBeginPlayFunction = FindScriptFunction(NewEnvironment, "BeginPlay");
    sol::protected_function NewTickFunction = FindScriptFunction(NewEnvironment, "Tick");
    sol::protected_function NewEndPlayFunction = FindScriptFunction(NewEnvironment, "EndPlay");
    sol::protected_function NewBeginOverlapFunction = FindScriptFunction(NewEnvironment, "OnOverlap");

    // 최초 시작에만 호출하며, 최초 로드 실패 후 재시도도 포함한다.
    if (bScriptReady == false && NewBeginPlayFunction.valid())
    {
        sol::protected_function_result Result = NewBeginPlayFunction();
        if (Result.valid() == false)
        {
            sol::error Error = Result;
            HTR_LOG(Error, "Lua BeginPlay failed ({}): {}", ScriptPath, Error.what());
            return false;
        }
    }

    // 준비가 끝난 뒤 환경과 모든 콜백을 교체한다.
    ScriptEnvironment = NewEnvironment;
    BeginPlayFunction = NewBeginPlayFunction;
    TickFunction = NewTickFunction;
    EndPlayFunction = NewEndPlayFunction;
    BeginOverlapFunction = NewBeginOverlapFunction;
    bScriptReady = true;

    return true;
}

void UScriptComponent::CheckScriptChanges(float DeltaTime)
{
    // 플레이 중이며 스크립트 경로가 있을 때만 검사한다.
    if (bCheckScriptChanges == false || ScriptPath.empty())
    {
        return;
    }

    // 매 프레임 파일을 조회하지 않고 0.5초 간격으로 확인한다.
    TimeSinceLastScriptCheck += DeltaTime;
    if (TimeSinceLastScriptCheck < 0.5f)
    {
        return;
    }

    TimeSinceLastScriptCheck = 0.0f;

    // 예외 대신 Error로 파일 조회 실패를 확인한다.
    std::error_code Error;
    const auto CurrentWriteTime = std::filesystem::last_write_time(ScriptPath, Error);
    if (Error)
    {
        // 기존 실행은 유지하고, 파일이 다시 나타나면 재로드한다.
        bHasAttemptedWriteTime = false;
        return;
    }

    if (bHasAttemptedWriteTime && CurrentWriteTime == LastAttemptedWriteTime)
    {
        return;
    }

    // 실패한 수정본도 기록하여 반복 실행과 로그 누적을 막는다.
    LastAttemptedWriteTime = CurrentWriteTime;
    bHasAttemptedWriteTime = true;

    TryLoadScript();
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


