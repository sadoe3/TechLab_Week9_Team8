#include "EnginePCH.h"
#include "TimeDilation.h"

void FTimeDilation::Tick()
{
	if(state == ESlomoState::Inactive) return;

	Elapsed += GetRealDelta();
	switch (state)
	{
	case ESlomoState::BlendIn:
		if (Elapsed >= BlendInTime) {
			state = ESlomoState::Hold;
			Dilation = TargetDilation;
			Elapsed -= BlendInTime;
			return;
		}

		{
			float Alpha = FMath::Clamp(Elapsed / BlendInTime, 0.0f, 1.0f);	// %
			float Eased = 1.0f - (1.0f - Alpha) * (1.0f - Alpha);
			Dilation = StartDilation + (TargetDilation - StartDilation) * Eased;
		}

		break;
	case ESlomoState::Hold:
		if (Elapsed >= HoldTime) {
			state = ESlomoState::BlendOut;
			StartDilation = Dilation;
			Elapsed -= HoldTime;
		}
		break;
	case ESlomoState::BlendOut:
		if (Elapsed >= BlendOutTime) {
			state = ESlomoState::Inactive;
			Dilation = 1.0f;
			Elapsed = 0.0f;
			return;
		}

		{
			float Alpha = FMath::Clamp(Elapsed / BlendOutTime, 0.0f, 1.0f);	// %
			float Eased = 1.0f - (1.0f - Alpha) * (1.0f - Alpha);
			Dilation = StartDilation + (1.0f - StartDilation) * Eased;
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

	StartDilation = Dilation;			// 보간 시작값
	state = ESlomoState::BlendIn;
}

