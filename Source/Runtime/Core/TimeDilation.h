#pragma once

#include "Core/EngineTimer.h"

enum class ESlomoState{
	Inactive,		// x 
	BlendIn,		// 1.0->목표치까지 내리기
	Hold,			// 목표값 유지
	BlendOut		// 목표->1.0으로 올리기
};

class FTimeDilation {
public:
	static void Tick();

	static void StartSlomo(float inTargetDilation, float inBlendInTime, float inHoldTime, float inBlendOutTime);
	
	static float GetDilation() { return Dilation; }
	static void SetDilation(float inDilation) { Dilation = inDilation; }
	static void ResetDilation() { Dilation = 1.0f; }

	static float GetRealDelta() { return EngineTimer::GetDeltaTime(); }	// 배속 영향x
	static float GetGameDelta() { return GetRealDelta() * Dilation; }		// 배속 영향o

private:
	inline static float Dilation = 1.0f;
	inline static ESlomoState state = ESlomoState::Inactive;

	inline static float Elapsed = 0.0f;
	inline static float TargetDilation = 1.0f;

	inline static float BlendInTime = 0.0f;
	inline static float HoldTime = 0.0f;
	inline static float BlendOutTime = 0.0f;

	inline static float StartDilation = 1.0f;
};	