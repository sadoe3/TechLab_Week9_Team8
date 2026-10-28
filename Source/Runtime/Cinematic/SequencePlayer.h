#pragma once

#include "EnginePCH.h"

enum class ESequenceAction {
	Wait, Finish
};

struct FSequenceStep {
	float Duration = 0.0f;
	ESequenceAction Action = ESequenceAction::Wait;
};


class FSequencePlayer {
public:
	static void Play(const TArray <FSequenceStep> & InSteps);
	static void Tick();
	static void Stop();
	static bool IsPlaying();

private:
	static void EnterStep();
	static void AdvanceStep();

	inline static TArray <FSequenceStep> Steps;
	inline static int32 CurrentIndex = 0;
	inline static  float Elapsed = 0.0f;

};	