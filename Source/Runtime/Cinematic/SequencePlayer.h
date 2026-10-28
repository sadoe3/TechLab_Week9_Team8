#pragma once

#include "EnginePCH.h"
#include "Engine/World.h"

enum class ESequenceAction {
	Wait, Finish, SetViewTarget
};

struct FSequenceStep {
	float Duration = 0.0f;
	ESequenceAction Action = ESequenceAction::Wait;
	FString TargetName;
};


class FSequencePlayer {
public:
	static void Play(const TArray <FSequenceStep> & InSteps, UWorld * World);
	static void Tick();
	static void Stop();
	static bool IsPlaying();

private:
	static void EnterStep();
	static void AdvanceStep();

	inline static TArray <FSequenceStep> Steps;
	inline static int32 CurrentIndex = 0;
	inline static  float Elapsed = 0.0f;

	inline static TWeakObjectPtr<UWorld> WorldPtr;

};	