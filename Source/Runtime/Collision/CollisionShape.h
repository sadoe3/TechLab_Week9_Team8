#pragma once

#include "Math/Transform.h"
#include "Math/Box.h"

enum class ECollisionShapeType : uint8
{
	Box,
	Sphere,
	Capsule,
	Convex,		// todo
	None
};

// Component 중심에 놓인 local 도형. Capsule의 축은 Z, HalfHeight는 반구 포함.
struct FCollisionLocalShape
{
	ECollisionShapeType Type = ECollisionShapeType::None;
	FVector BoxExtent = FVector::ZeroVector;
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
};

// Physics Scene에서 Collision 계산을 위해 사용되는 world 도형.
struct FCollisionWorldShape
{
	ECollisionShapeType Type = ECollisionShapeType::None;
	FVector Center = FVector::ZeroVector;
	FQuat Rotation;
	FVector BoxExtent = FVector::ZeroVector;
	float Radius = 0.0f;
	float HalfHeight = 0.0f;

	static FCollisionWorldShape CreateFromLocal(const FCollisionLocalShape& LocalShape, const FTransform& WorldTransform);
	FBox CalcBounds() const;
	bool IsSupported() const;
	FVector GetSupportPoint(const FVector& Direction) const;
	// GJK/EPA는 Sphere의 점, Capsule의 선분을 검사한 뒤 반지름을 더한다.
	FVector GetCoreSupportPoint(const FVector& Direction) const;
	float GetMargin() const;
};
