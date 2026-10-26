#pragma once

#include "Types.h"
#include "EngineString.h"


class FGameHud
{
public:
	static int32 GetScore() { return Score; }
	static void SetScore(int32 inScore) { Score = inScore; }
	static void AddScore(int32 inScore) { Score += inScore; }

private:
	static int32 Score;
};
