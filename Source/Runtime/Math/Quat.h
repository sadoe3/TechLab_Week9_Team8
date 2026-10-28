#pragma once

#include "VectorRegister.h"

using FQuatRegister = FVectorRegister;

//template<typename T>
//struct TVector;
struct FVector;
struct FMatrix;
struct FRotator;

struct FQuat
{
	union
	{
		float V[4];
		struct {
			float X;
			float Y;
			float Z;
			float W;
		};
	};

	FQuat();
	FQuat(float InX, float InY, float InZ, float InW = 1.0f);
	FQuat(FRotator InRotator);

	/* Function */
	FQuat operator * (const FQuat& Other) const;
	FQuat Conjugate() const;
	FQuat UnitInverse() const;
	FQuat Inverse() const;
	FQuat Normalized() const;
	bool Equals(const FQuat& Other, float Tolerance = 1.e-6f) const;
	FVector RotateVector(const FVector& V) const;
	FMatrix ToFMatrix() const;
	FRotator ToFRotator() const;

	FVector GetForwardVector() const;
	FVector GetRightVector() const;
	FVector GetUpVector() const;


	/* Static */
	static FQuat Identity();
	// 정규화된 회전 사이의 짧은 경로를 보간한다. Alpha는 [0, 1].
	static FQuat Slerp(const FQuat& A, const FQuat& B, float Alpha);
	static FQuat MakeFromAxisAngle(const FVector& Axis, float AngleRadians);
	static FQuat MakeFromEuler(float Roll, float Pitch, float Yaw);
};


