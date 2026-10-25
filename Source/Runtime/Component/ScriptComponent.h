#pragma once

#include <filesystem>
#include <sol/sol.hpp>

#include "ActorComponent.h"
#include "Core/EngineString.h"

class UPrimitiveComponent;
struct FHitResult;

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
    void OnEndOverlap(AActor* OtherActor);

    // 후보 파일 실행 여부
    bool IsLoadingScript() const
    {
        return bLoadingScript;
    }

private:
    // 컴포넌트의 Lua 참조 해제
    void ResetScript();

    // 지정한 환경에 직접 정의된 콜백만 찾는다.
    static sol::protected_function FindScriptFunction(
        const sol::environment& Environment,
        const char* FunctionName);

    // 새 환경과 콜백을 준비하고, 성공했을 때만 교체한다.
    bool TryLoadScript();

    // 일정 간격으로 파일 수정 시각을 확인하고 재로드를 시도한다.
    void CheckScriptChanges(float DeltaTime);

    void HandleComponentBeginOverlap(UPrimitiveComponent* thisComp, UPrimitiveComponent* OtherComp, bool, const FHitResult& HIt);
    void HandleComponentEndOverlap(UPrimitiveComponent* thisComp, UPrimitiveComponent* OtherComp);


    FString ScriptPath;
    sol::environment ScriptEnvironment;

    sol::protected_function BeginPlayFunction;
    sol::protected_function TickFunction;
    sol::protected_function EndPlayFunction;
    sol::protected_function BeginOverlapFunction;
    sol::protected_function EndOverlapFunction;
    bool bScriptReady = false;

    // 실패한 수정본도 기록하여 같은 파일의 반복 로드를 막는다.
    std::filesystem::file_time_type LastAttemptedWriteTime{};
    bool bHasAttemptedWriteTime = false;

    float TimeSinceLastScriptCheck = 0.0f;

    // 최초 로드에 실패해도 수정 후 재시도할 수 있도록 유지한다.
    bool bCheckScriptChanges = false;

    // 후보 파일 실행 중 코루틴 생성 차단
    bool bLoadingScript = false;
};

