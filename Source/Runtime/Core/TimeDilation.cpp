#include "EnginePCH.h"
#include "TimeDilation.h"

void FTimeDilation::Tick()
{
	if(State == ESlomoState::Inactive) return;

	Elapsed += GetRealDelta();
	switch (State)
	{
	case ESlomoState::BlendIn:
		if (Elapsed >= BlendInTime) {
			State = ESlomoState::Hold;
			Dilation = TargetDilation;
			Elapsed -= BlendInTime;
			return;
		}

		{
			float Alpha = FMath::Clamp(Elapsed / BlendInTime, 0.0f, 1.0f);	// %
			float Eased = 1.0f - (1.0f - Alpha) * (1.0f - Alpha);
			Dilation = InitialDilation + (TargetDilation - InitialDilation) * Eased;
		}

		break;
	case ESlomoState::Hold:
		if (Elapsed >= HoldTime) {
			State = ESlomoState::BlendOut;
			InitialDilation = Dilation;
			Elapsed -= HoldTime;
		}
		break;
	case ESlomoState::BlendOut:
		if (Elapsed >= BlendOutTime) {
			State = ESlomoState::Inactive;
			Dilation = 1.0f;
			Elapsed = 0.0f;
			OnSlomoFinished.Broadcast();
			return;
		}

		{
			float Alpha = FMath::Clamp(Elapsed / BlendOutTime, 0.0f, 1.0f);	// %
			float Eased = 1.0f - (1.0f - Alpha) * (1.0f - Alpha);
			Dilation = InitialDilation + (1.0f - InitialDilation) * Eased;
		}

		break;
	default:
		break;
	}

}

void FTimeDilation::StartSlomo(float inTargetDilation, float inBlendInTime, float inHoldTime, float inBlendOutTime)
{
	Elapsed = 0.0f;						// 흐른 시간
	TargetDilation = inTargetDilation;	// 목표

	BlendInTime = inBlendInTime;		// 진입 시간
	HoldTime = inHoldTime;				// 유지 시간 
	BlendOutTime = inBlendOutTime;		// 복귀 시간

	InitialDilation = Dilation;			// 보간 시작값
	State = ESlomoState::BlendIn;
}

