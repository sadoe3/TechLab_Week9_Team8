#pragma once

#include "Types.h"
#include "EngineString.h"
#include "Core/Misc/Delegate.h"

DECLARE_DELEGATE(ScoreUpdate, int32);
DECLARE_DELEGATE(CountdownUpdate, const FString&);

class FGameHud
{
public:
	static int32 GetScore() { return Score; }
	static void SetScore(int32 inScore) {
		Score = inScore;
		OnScoreChanged.Broadcast(Score);
	}
	static void AddScore(int32 inScore) { Score += inScore; }

	inline static ScoreUpdate OnScoreChanged;

	static const FString& GetCountdown() { return Countdown; }
	static void SetCountdown(const FString& inCountdown) {
		Countdown = inCountdown;
		OnCountdownChanged.Broadcast(Countdown);
	}

	inline static CountdownUpdate OnCountdownChanged;

private:
	inline static int32 Score = 0;
	inline static FString Countdown;
};
