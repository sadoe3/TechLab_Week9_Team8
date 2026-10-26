#pragma once

#include "Types.h"
#include "EngineString.h"
#include "Core/Misc/Delegate.h"

DECLARE_DELEGATE(ScoreUpdate, int32);

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

private:
	inline static int32 Score = 0;
};
