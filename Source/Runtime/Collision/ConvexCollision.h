#pragma once

#include "CollisionShape.h"

// Success는 정상 종료를 뜻하며 접촉 여부는 별도 결과 필드로 확인한다.
enum class ECollisionQueryStatus : uint8 { Success, InvalidShape, NotConverged };

struct FSupportPoint
{
	FVector PointA;
	FVector PointB;
	FVector Difference;
};

struct FGJKSimplex
{
	// PointA/PointB/Difference는 질의 A의 중심을 원점으로 한 좌표다.
	FSupportPoint Points[4];
	int32 Count = 0;
};

struct FGJKResult
{
	ECollisionQueryStatus Status = ECollisionQueryStatus::NotConverged;
	bool bIntersecting = false; // 허용 오차 내 표면 접촉도 포함한다.
	float Distance = 0.0f;
	FVector PointA;
	FVector PointB;
	FVector Normal; // B에서 A 방향
	FGJKSimplex Simplex;
	float CoreDistance = 0.0f;
	FVector CorePointA;
	FVector CorePointB;
};

struct FPenetrationResult
{
	ECollisionQueryStatus Status = ECollisionQueryStatus::NotConverged;
	float Depth = 0.0f;
	FVector Normal; // A를 B 밖으로 빼내는 방향
	FVector PointA;
	FVector PointB;
};

struct FShapeSweepResult
{
	ECollisionQueryStatus Status = ECollisionQueryStatus::NotConverged;
	bool bHit = false;
	bool bStartPenetrating = false;
	float Time = 1.0f;
	float PenetrationDepth = 0.0f;
	FVector Normal;
	FVector ImpactPoint;
};

namespace GJK
{
	// 거리와 겹침 판정은 같은 simplex 계산을 공유한다.
	FGJKResult CalculateDistance(const FCollisionWorldShape& A, const FCollisionWorldShape& B);
	FShapeSweepResult Sweep(const FCollisionWorldShape& A, const FCollisionWorldShape& B, const FVector& Delta);
}

namespace EPA
{
	// GJKResult는 동일한 A/B에 대한 성공한 GJK::CalculateDistance 결과여야 한다.
	FPenetrationResult CalculatePenetration(const FCollisionWorldShape& A, const FCollisionWorldShape& B, const FGJKResult& GJKResult);
}
