
#include "EnginePCH.h"
#include "ScriptComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Scripting/LuaRuntime.h"

#include "UObject/Casts.h"
#include "PrimitiveComponent.h"

#include "UObject/WeakObjectPtr.h"

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

    // 에디터 실행 제외
    if (OwnerWorld->GetWorldType() == EWorldType::Editor)
    {
        return;
    }

    for (auto Comp : OwnerActor->GetComponents()) {
        UPrimitiveComponent *comp = Cast <UPrimitiveComponent> (Comp);
        if (comp == nullptr) continue;

        comp->OnComponentBeginOverlap.AddRaw(this, &UScriptComponent::HandleComponentBeginOverlap);
        comp->OnComponentEndOverlap.AddRaw(this, &UScriptComponent::HandleComponentEndOverlap);
        comp->OnComponentHit.AddRaw(this, &UScriptComponent::HandleComponentHit);
    }

    // 최초 로드 전 변경 감시 활성화
    bHasAttemptedWriteTime = false;
    bCheckScriptChanges = true;

    // 공통 검사 경로로 최초 로드
    TimeSinceLastScriptCheck = 0.5f;
    CheckScriptChanges(0.0f);
}

void UScriptComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

	// 스크립트 변경 확인
	CheckScriptChanges(DeltaTime);
	if (!bScriptReady||!CanRunGameplay())
	{
		return;
	}

	FLuaRuntime* Runtime = GetOwner()->GetWorld()->GetLuaRuntime();
	assert(Runtime != nullptr);

	// Lua Tick 실행
	if (TickFunction.valid())
	{
		Runtime->Call(TickFunction, ScriptPath, "Tick", DeltaTime);
	}

	// 소유 컴포넌트 Tick에서 코루틴 재개
	Runtime->TickCoroutines(this, DeltaTime);
}

void UScriptComponent::EndPlay()
{
    // 시작 준비가 완료된 스크립트만 종료 콜백을 호출한다.
    const bool bCallEndPlay = bScriptReady && EndPlayFunction.valid();

    // 종료 중 재로드나 다른 Lua 콜백이 실행되지 않도록 막는다.
    bCheckScriptChanges = false;
    bScriptReady = false;
    PrimaryComponentTick.UnRegisterTickFunction();

    // 소유 Coroutine 취소
    FLuaRuntime* Runtime = GetOwner()->GetWorld()->GetLuaRuntime();
    if (Runtime)
    {
        Runtime->CancelCoroutines(this);
    }

    // 참조를 해제하기 전에 Lua EndPlay()를 실행한다.
    if (bCallEndPlay)
    {
        assert(Runtime != nullptr);
        Runtime->Call(EndPlayFunction, ScriptPath, "EndPlay");
    }

    AActor* OwnerActor = GetOwner();
    assert(OwnerActor != nullptr);

    for (auto Comp : OwnerActor->GetComponents()) {
        UPrimitiveComponent *comp = Cast <UPrimitiveComponent>(Comp);
        if (comp == nullptr) continue;

        comp->OnComponentBeginOverlap.RemoveAll(this);
        comp->OnComponentEndOverlap.RemoveAll(this);
        comp->OnComponentHit.RemoveAll(this);
    }

    ResetScript();
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

    FLuaRuntime* Runtime = OwnerWorld->GetLuaRuntime();
    assert(Runtime != nullptr);

    
    // 기존 실행 환경을 수정하지 않고 교체 후보를 만든다.
    sol::environment NewEnvironment = Runtime->CreateEnvironment(OwnerActor);
    Runtime->BindCoroutineFunctions(NewEnvironment, this, ScriptPath);


    // 후보 파일 실행 중 코루틴 생성 차단
    bLoadingScript = true;
    const bool bLoaded = Runtime->LoadScript(ScriptPath, NewEnvironment);
    bLoadingScript = false;

    // 로드 실패 시 지역 변수만 해제되고 기존 실행은 유지된다.
    if (!bLoaded)
    {
        return false;
    }

    // 없는 콜백도 빈 참조로 준비하여 삭제한 이전 함수가 남지 않게 한다.
    sol::protected_function NewBeginPlayFunction = FindScriptFunction(NewEnvironment, "BeginPlay");
    sol::protected_function NewTickFunction = FindScriptFunction(NewEnvironment, "Tick");
    sol::protected_function NewEndPlayFunction = FindScriptFunction(NewEnvironment, "EndPlay");
    sol::protected_function NewBeginOverlapFunction = FindScriptFunction(NewEnvironment, "OnBeginOverlap");
    sol::protected_function NewEndOverlapFunction = FindScriptFunction(NewEnvironment, "OnEndOverlap");
    sol::protected_function NewHitFunction = FindScriptFunction(NewEnvironment, "OnHit");

    // 최초 시작에만 호출하며, 최초 로드 실패 후 재시도도 포함한다.
    if (bScriptReady == false && NewBeginPlayFunction.valid())
    {
        sol::protected_function_result Result = NewBeginPlayFunction();
        if (Result.valid() == false)
        {
            sol::error Error = Result;
            HTR_LOG(Error, "Lua BeginPlay failed ({}): {}", ScriptPath, Error.what());
            // 실패한 시작에서 생성한 작업 취소
            Runtime->CancelCoroutines(this);
            return false;
        }
    }

    // 후보 콜백의 종료·삭제 요청 반영
    if (!CanRunGameplay())
    {
        Runtime->CancelCoroutines(this);
        return false;
    }

    // 교체 전 스크립트 작업 취소
    if (bScriptReady)
    {
        Runtime->CancelCoroutines(this);
    }

    // 준비가 끝난 뒤 환경과 모든 콜백을 교체한다.
    ScriptEnvironment = NewEnvironment;
    BeginPlayFunction = NewBeginPlayFunction;
    TickFunction = NewTickFunction;
    EndPlayFunction = NewEndPlayFunction;
    BeginOverlapFunction = NewBeginOverlapFunction;
    EndOverlapFunction = NewEndOverlapFunction;
    HitFunction = NewHitFunction;
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

void UScriptComponent::HandleComponentBeginOverlap(UPrimitiveComponent* thisComp, UPrimitiveComponent* OtherComp, bool, const FHitResult& HIt)
{
    AActor* OtherActor = OtherComp->GetOwner();
    if (OtherActor == nullptr) return;

    assert(OtherComp != nullptr);

    OnBeginOverlap(OtherActor);
}

void UScriptComponent::HandleComponentEndOverlap(UPrimitiveComponent* thisComp, UPrimitiveComponent* OtherComp)
{
    AActor* OtherActor = OtherComp->GetOwner();
    if (OtherActor == nullptr) return;

    assert(OtherComp != nullptr);

    OnEndOverlap(OtherActor);
}

void UScriptComponent::HandleComponentHit(UPrimitiveComponent* thisComp, UPrimitiveComponent* OtherComp, const FHitResult& Hit){
    AActor* OtherActor = OtherComp->GetOwner();
    if (OtherActor == nullptr) return;

    assert(OtherComp != nullptr);

    OnHit(OtherActor);
}

void UScriptComponent::OnBeginOverlap(AActor* OtherActor)
{
    assert(OtherActor != nullptr);

    if (!bScriptReady || !CanRunGameplay() || !BeginOverlapFunction.valid())
    {
        return;
    }

    // Lua의 OnBeginOverlap(OtherActor)을 호출한다.
    FLuaRuntime* Runtime = GetOwner()->GetWorld()->GetLuaRuntime();
    assert(Runtime != nullptr);

    Runtime->Call(BeginOverlapFunction, ScriptPath, "OnBeginOverlap", TWeakObjectPtr<AActor>(OtherActor));
}

void UScriptComponent::OnEndOverlap(AActor* OtherActor)
{
    assert(OtherActor != nullptr);

    if (!bScriptReady || !CanRunGameplay() || !EndOverlapFunction.valid())
    {
        return;
    }

    // 겹침 종료 콜백 실행
    FLuaRuntime* Runtime = GetOwner()->GetWorld()->GetLuaRuntime();
    assert(Runtime != nullptr);

    Runtime->Call(EndOverlapFunction, ScriptPath, "OnEndOverlap", TWeakObjectPtr<AActor>(OtherActor));
}

void UScriptComponent::OnHit(AActor* OtherActor)
{
    assert(OtherActor != nullptr);

    if (!bScriptReady || !CanRunGameplay() || !HitFunction.valid())
    {
        return;
    }

    // 겹침 종료 콜백 실행
    FLuaRuntime* Runtime = GetOwner()->GetWorld()->GetLuaRuntime();
    assert(Runtime != nullptr);

    Runtime->Call(HitFunction, ScriptPath, "OnHit", TWeakObjectPtr<AActor>(OtherActor));
}

void UScriptComponent::ResetScript()
{
    // 빈 객체로 교체하여 컴포넌트가 가진 Lua 참조를 해제한다.
    BeginPlayFunction = sol::protected_function();
    TickFunction = sol::protected_function();
    EndPlayFunction = sol::protected_function();
    BeginOverlapFunction = sol::protected_function();
    EndOverlapFunction = sol::protected_function();
    HitFunction = sol::protected_function();

    // Environment 해제
    ScriptEnvironment = sol::environment();
}