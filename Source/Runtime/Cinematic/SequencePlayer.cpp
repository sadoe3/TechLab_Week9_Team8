#include "EnginePCH.h"
#include "SequencePlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraTypes.h"
#include "Core/TimeDilation.h"
#include "Core/EngineLog.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"

namespace
{
	AActor* FindActorByName(UWorld* World, const FString& Name)
	{
		if (World == nullptr) return nullptr;

		ULevel* Level = World->GetPersistentLevel();
		if (Level == nullptr) return nullptr;

		const FName SearchName(Name);

		for (AActor* Actor : Level->GetActors()) {
			if (Actor == nullptr || Actor->IsPendingDestroy() || Actor->GetFName() != SearchName) continue;
			return Actor;
		}
		return nullptr;
	}

	float GetParam(const FSequenceStep& Step, const FString& Key, float Default)
	{
		const float* Found = Step.Params.Find(Key);
		return Found ? *Found : Default;
	}
}

void FSequencePlayer::Play(const TArray<FSequenceStep>& InSteps, UWorld* World)
{
	Steps = InSteps;
	CurrentIndex = 0;
	Elapsed = 0.0f;

	WorldPtr = World;

	EnterStep();
}

void FSequencePlayer::Tick()
{
	if (!IsPlaying()) return;

	Elapsed += FTimeDilation::GetRealDelta();

	while (IsPlaying() && Elapsed >= Steps[CurrentIndex].Duration)
		AdvanceStep();

	if (IsPlaying()) TickStep();
}

void FSequencePlayer::TickStep()
{
	const FSequenceStep& Step = Steps[CurrentIndex];
	if (Step.Action != ESequenceAction::MoveActor) return;

	AActor* Actor = StepActor.Get();
	if (Actor == nullptr) return;

	USceneComponent* Root = StepActor->GetRootComponent();
	if (Root == nullptr) return;

	float Alpha;
	if (Step.Duration <= 0) Alpha = 1.0f;
	else Alpha = Elapsed / Step.Duration;

	FViewTargetTransitionParams Blend;
	Blend.BlendFunction = EViewTargetBlendFunction::Cubic;
	Alpha = Blend.GetBlendAlpha(Alpha);

	const float ToX = GetParam(Step, "ToX", StepStartLocation.X);
	const float ToY = GetParam(Step, "ToY", StepStartLocation.Y);
	const float ToZ = GetParam(Step, "ToZ", StepStartLocation.Z);


	const FVector NewLocation(
		StepStartLocation.X + (ToX - StepStartLocation.X) * Alpha,
		StepStartLocation.Y + (ToY - StepStartLocation.Y) * Alpha,
		StepStartLocation.Z + (ToZ - StepStartLocation.Z) * Alpha);

	// 연출용 이동이라 충돌로 막히지 않게 Sweep을 끈다.
	Root->SetWorldLocation(NewLocation, false);
	
}

void FSequencePlayer::Stop()
{
	Steps.Empty();
	WorldPtr.Reset();
	CurrentIndex = 0;
	Elapsed = 0.0f;
	StepActor.Reset();
}

bool FSequencePlayer::IsPlaying()
{
	return CurrentIndex < Steps.Num();
}

void FSequencePlayer::EnterStep()
{
	if (!IsPlaying()) return;

	const FSequenceStep& Step = Steps[CurrentIndex];
	HTR_LOG(Info, "CurrentIndex : {} , Action : {} ", CurrentIndex, (int32)Steps[CurrentIndex].Action);

	switch (Step.Action) {
	case ESequenceAction::Wait: break;
	case ESequenceAction::Finish: break;
	case ESequenceAction::SetViewTarget:
	{
		UWorld* World = WorldPtr.Get();
		if (World == nullptr)
			HTR_LOG(Error, "[Sequence] Not Found World");
		AActor* Target = FindActorByName(World, Step.TargetName);
		if (Target == nullptr)
		{
			HTR_LOG(Error, "[Sequence] Target not found: {}", Step.TargetName);
			break;
		}


		World->SetViewTarget(Target);
		break;
	}

	case ESequenceAction::Fade:
	{
		UWorld* World = WorldPtr.Get();
		if (World == nullptr) break;

		APlayerCameraManager* Manager = World->GetPlayerCameraManager();
		if (Manager == nullptr) break;

		const float From = GetParam(Step, "From", 0.0f);
		const float To = GetParam(Step, "To", 1.0f);
		Manager->StartCameraFade(From, To, Step.Duration, FLinearColor(0, 0, 0), true);
		break;
	}
	case ESequenceAction::LetterBox:
	{
		UWorld* World = WorldPtr.Get();
		if (World == nullptr) break;

		APlayerCameraManager* Manager = World->GetPlayerCameraManager();
		if (Manager == nullptr) break;

		const float Ratio = GetParam(Step, "Ratio", 0.12f);
		Manager->StartCameraLetterBox(Ratio, Step.Duration);
		break;
	}
	case ESequenceAction::MoveActor:
	{
		UWorld* World = WorldPtr.Get();
		if (World == nullptr) break;

		AActor* TargetActor = FindActorByName(World, Step.TargetName);
		if (TargetActor == nullptr) {
			HTR_LOG(Error, "[Sequence] targetActor not found: {}", Step.TargetName);
			break;
		}

		StepActor = TargetActor;

		StepStartLocation = TargetActor->GetActorLocation();
		break;
	}

	}

	

}

void FSequencePlayer::AdvanceStep()
{
	Elapsed -= Steps[CurrentIndex].Duration;
	CurrentIndex++;
	if (!IsPlaying()) {
		Stop();
		return;
	}
	else {
		EnterStep();
	}
}
