#pragma once

#include "Core/EngineTimer.h"

class FTimeDilation {
public:
	static float GetDilation() { return Dilation; }
	static void SetDilation(float inDilation) { Dilation = inDilation; }
	static void ResetDilation() { Dilation = 1.0f; }

	static float GetRealDelta() { return EngineTimer::GetDeltaTime(); }	// 배속 영향x
	static float GetGameDelta() { return GetRealDelta() * Dilation; }		// 배속 영향o

private:
	inline static float Dilation = 1.0f;
	
};