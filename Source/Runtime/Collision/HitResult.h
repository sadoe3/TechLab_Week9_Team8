#pragma once
#include "Core/Types.h"
#include "Math/Vector.h"

class UPrimitiveComponent;

struct FOverlapResult
{
	UPrimitiveComponent* Component = nullptr;
	bool bBlockingHit = false;
};

struct FHitResult
{
	int32 FaceIndex = -1; // Hitted face index

	float Time; // For swept
	float Distance = FLT_MAX;

	FVector Location = FVector(); // For swept
	FVector ImpactPoint = FVector();
	FVector Normal; //For swept
	FVector ImpactNormal;

	FVector TraceStart;
	FVector TraceEnd;

	float PenetrationDepth = 0.0f;

	bool bBlockingHit = false;
	bool bStartPenetrating = false;

	UPrimitiveComponent* HitComponent = nullptr;

	FHitResult()
	{
		Init();
	}

	explicit FHitResult(float InTime)
	{
		Init();
		Time = InTime;
	}

	// Initailize empty hit result
	inline void Init()
	{
		Time = 1.0f;		
	}

	inline void Init(FVector Start, FVector End)
	{
		Time = 1.0f;
		TraceStart = Start;
		TraceEnd = End;
	}
};
