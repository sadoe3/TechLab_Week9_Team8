#include "EnginePCH.h"
#include "SequencePlayer.h"
#include "Core/TimeDilation.h"
#include "Core/EngineLog.h"

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
}

void FSequencePlayer::Stop()
{
	Steps.Empty();
	WorldPtr.Reset();
	CurrentIndex = 0;
	Elapsed = 0.0f;
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
		HTR_LOG(Info, "[Sequence] SetViewTarget: {}", Step.TargetName);
		break;
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
