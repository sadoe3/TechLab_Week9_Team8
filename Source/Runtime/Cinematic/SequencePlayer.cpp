#include "EnginePCH.h"
#include "SequencePlayer.h"
#include "Core/TimeDilation.h"
#include "Core/EngineLog.h"

void FSequencePlayer::Play(const TArray<FSequenceStep>& InSteps)
{
	Steps = InSteps;
	CurrentIndex = 0;
	Elapsed = 0.0f;

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

	HTR_LOG(Info, "CurrentIndex : {} , Action : {} ", CurrentIndex, (int32)Steps[CurrentIndex].Action);

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
