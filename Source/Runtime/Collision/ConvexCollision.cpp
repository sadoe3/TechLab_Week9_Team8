#include "EnginePCH.h"
#include "ConvexCollision.h"
#include <array>
#include "Containers/Array.h"

namespace
{
constexpr int32 GJKIterationLimit = 64;
constexpr int32 EPAIterationLimit = 128;
constexpr int32 SweepIterationLimit = 64;
constexpr float AbsoluteTolerance = 1.0e-5f;
constexpr float RelativeTolerance = 1.0e-6f;
constexpr double BarycentricTolerance = 1.0e-9;

float CalculateTolerance(const FCollisionWorldShape& A, const FCollisionWorldShape& B)
{
	const float Scale = std::max({A.BoxExtent.Size(), B.BoxExtent.Size(), A.Radius, B.Radius, A.HalfHeight, B.HalfHeight});
	return AbsoluteTolerance + RelativeTolerance * Scale;
}

// 모든 simplex 점은 A.Center를 원점으로 삼는다. 큰 월드 좌표의 차감 오차를 줄인다.
FSupportPoint GetSupportPoint(const FCollisionWorldShape& A, const FCollisionWorldShape& B, const FVector& Direction)
{
	FCollisionWorldShape LocalA = A;
	FCollisionWorldShape LocalB = B;
	LocalA.Center = FVector(0);
	LocalB.Center = FVector(0);
	const FVector PointA = LocalA.GetCoreSupportPoint(Direction);
	const FVector PointB = LocalB.GetCoreSupportPoint(-Direction) + (B.Center - A.Center);
	return {PointA, PointB, PointA - PointB};
}

// 선택한 점들의 affine 공간에 원점을 투영하여 barycentric 좌표를 구한다.
bool ProjectOrigin(const FSupportPoint* Points, const int32* PointIndices, int32 PointCount, double* Weights)
{
	Weights[0] = 1.0;
	if (PointCount == 1) return true;
	double Edges[3][3] = {};
	for (int32 CurrentEdgeIndex = 0; CurrentEdgeIndex < PointCount - 1; ++CurrentEdgeIndex)
		for (int32 CurrentAxis = 0; CurrentAxis < 3; ++CurrentAxis)
			Edges[CurrentEdgeIndex][CurrentAxis] = double(Points[PointIndices[CurrentEdgeIndex + 1]].Difference[CurrentAxis]) - Points[PointIndices[0]].Difference[CurrentAxis];
	double Matrix[3][4] = {};
	double Scale = 0.0;
	const int32 EdgeCount = PointCount - 1;
	for (int32 CurrentRow = 0; CurrentRow < EdgeCount; ++CurrentRow)
	{
		for (int32 CurrentColumn = 0; CurrentColumn < EdgeCount; ++CurrentColumn)
		{
			for (int32 CurrentAxis = 0; CurrentAxis < 3; ++CurrentAxis)
				Matrix[CurrentRow][CurrentColumn] += Edges[CurrentRow][CurrentAxis] * Edges[CurrentColumn][CurrentAxis];
			Scale = std::max(Scale, std::abs(Matrix[CurrentRow][CurrentColumn]));
		}
		for (int32 CurrentAxis = 0; CurrentAxis < 3; ++CurrentAxis)
			Matrix[CurrentRow][EdgeCount] -= Edges[CurrentRow][CurrentAxis] * Points[PointIndices[0]].Difference[CurrentAxis];
	}
	for (int32 CurrentColumn = 0; CurrentColumn < EdgeCount; ++CurrentColumn)
	{
		int32 PivotRow = CurrentColumn;
		for (int32 CurrentRow = CurrentColumn + 1; CurrentRow < EdgeCount; ++CurrentRow)
			if (std::abs(Matrix[CurrentRow][CurrentColumn]) > std::abs(Matrix[PivotRow][CurrentColumn])) PivotRow = CurrentRow;
		if (std::abs(Matrix[PivotRow][CurrentColumn]) <= Scale * 1.0e-12) return false;
		for (int32 CurrentEntryColumn = CurrentColumn; CurrentEntryColumn <= EdgeCount; ++CurrentEntryColumn) std::swap(Matrix[CurrentColumn][CurrentEntryColumn], Matrix[PivotRow][CurrentEntryColumn]);
		const double Divisor = Matrix[CurrentColumn][CurrentColumn];
		for (int32 CurrentEntryColumn = CurrentColumn; CurrentEntryColumn <= EdgeCount; ++CurrentEntryColumn) Matrix[CurrentColumn][CurrentEntryColumn] /= Divisor;
		for (int32 CurrentRow = 0; CurrentRow < EdgeCount; ++CurrentRow)
		{
			if (CurrentRow == CurrentColumn) continue;
			const double Factor = Matrix[CurrentRow][CurrentColumn];
			for (int32 CurrentEntryColumn = CurrentColumn; CurrentEntryColumn <= EdgeCount; ++CurrentEntryColumn) Matrix[CurrentRow][CurrentEntryColumn] -= Factor * Matrix[CurrentColumn][CurrentEntryColumn];
		}
	}
	for (int32 CurrentEdgeIndex = 0; CurrentEdgeIndex < EdgeCount; ++CurrentEdgeIndex)
	{
		Weights[CurrentEdgeIndex + 1] = Matrix[CurrentEdgeIndex][EdgeCount];
		Weights[0] -= Weights[CurrentEdgeIndex + 1];
	}
	return true;
}

// 최대 4점의 정점/변/면/내부를 검사한다. 퇴화한 면은 더 낮은 차원의 부분집합이 처리한다.
FVector FindClosestPoint(FGJKSimplex& Simplex, FVector& PointA, FVector& PointB)
{
	double BestDistanceSquared = DBL_MAX;
	double BestWeights[4] = {};
	FVector BestPosition(0);
	for (int32 CurrentSubsetMask = 1; CurrentSubsetMask < (1 << Simplex.Count); ++CurrentSubsetMask)
	{
		int32 SubsetPointIndices[4], SubsetPointCount = 0;
		for (int32 CurrentPointIndex = 0; CurrentPointIndex < Simplex.Count; ++CurrentPointIndex)
			if (CurrentSubsetMask & (1 << CurrentPointIndex)) SubsetPointIndices[SubsetPointCount++] = CurrentPointIndex;
		double Weights[4] = {};
		if (!ProjectOrigin(Simplex.Points, SubsetPointIndices, SubsetPointCount, Weights)) continue;
		bool bInsideSubset = true;
		double WeightSum = 0.0;
		for (int32 CurrentPointIndex = 0; CurrentPointIndex < SubsetPointCount; ++CurrentPointIndex)
		{
			if (Weights[CurrentPointIndex] < -BarycentricTolerance) bInsideSubset = false;
			Weights[CurrentPointIndex] = std::max(Weights[CurrentPointIndex], 0.0);
			WeightSum += Weights[CurrentPointIndex];
		}
		if (!bInsideSubset) continue;
		double Position[3] = {};
		for (int32 CurrentPointIndex = 0; CurrentPointIndex < SubsetPointCount; ++CurrentPointIndex)
			for (int32 CurrentAxis = 0; CurrentAxis < 3; ++CurrentAxis)
				Position[CurrentAxis] += Simplex.Points[SubsetPointIndices[CurrentPointIndex]].Difference[CurrentAxis] * Weights[CurrentPointIndex] / WeightSum;
		const double DistanceSquared = Position[0] * Position[0] + Position[1] * Position[1] + Position[2] * Position[2];
		if (DistanceSquared >= BestDistanceSquared) continue;
		BestDistanceSquared = DistanceSquared;
		BestPosition = FVector(float(Position[0]), float(Position[1]), float(Position[2]));
		std::fill(std::begin(BestWeights), std::end(BestWeights), 0.0);
		for (int32 CurrentPointIndex = 0; CurrentPointIndex < SubsetPointCount; ++CurrentPointIndex) BestWeights[SubsetPointIndices[CurrentPointIndex]] = Weights[CurrentPointIndex] / WeightSum;
	}
	FGJKSimplex ReducedSimplex;
	PointA = PointB = FVector(0);
	for (int32 CurrentPointIndex = 0; CurrentPointIndex < Simplex.Count; ++CurrentPointIndex)
	{
		if (BestWeights[CurrentPointIndex] <= 0.0) continue;
		const float Weight = static_cast<float>(BestWeights[CurrentPointIndex]);
		PointA += Simplex.Points[CurrentPointIndex].PointA * Weight;
		PointB += Simplex.Points[CurrentPointIndex].PointB * Weight;
		ReducedSimplex.Points[ReducedSimplex.Count++] = Simplex.Points[CurrentPointIndex];
	}
	Simplex = ReducedSimplex;
	return BestPosition;
}

bool ContainsPoint(const TArray<FSupportPoint>& Points, const FSupportPoint& Point, float Epsilon)
{
	for (const FSupportPoint& CurrentPoint : Points)
		if (FVector::Dot(CurrentPoint.Difference - Point.Difference, CurrentPoint.Difference - Point.Difference) <= double(Epsilon) * Epsilon) return true;
	return false;
}

struct FEPAFace
{
	int32 A, B, C;
	FVector Normal;
};

bool MakeFace(const TArray<FSupportPoint>& Points, int32 A, int32 B, int32 C, FEPAFace& Face)
{
	FVector Normal = (Points[B].Difference - Points[A].Difference).Cross(Points[C].Difference - Points[A].Difference);
	if (FVector::Dot(Normal, Normal) == 0.0) return false;
	Normal = Normal.GetSafeNormal(FLT_MIN);
	if (FVector::Dot(Normal, Points[A].Difference) < 0.0)
	{
		std::swap(B, C);
		Normal = -Normal;
	}
	Face = {A, B, C, Normal};
	return true;
}
}

FGJKResult GJK::CalculateDistance(const FCollisionWorldShape& A, const FCollisionWorldShape& B)
{
	FGJKResult Result;
	if (!A.IsSupported() || !B.IsSupported())
	{
		Result.Status = ECollisionQueryStatus::InvalidShape;
		return Result;
	}
	const float Epsilon = CalculateTolerance(A, B);
	const float Margin = A.GetMargin() + B.GetMargin();
	Result.Simplex.Points[0] = GetSupportPoint(A, B, (B.Center - A.Center).GetSafeNormal(FLT_MIN, FVector(1, 0, 0)));
	Result.Simplex.Count = 1;
	FVector CoreA, CoreB, Difference;
	for (int32 CurrentIteration = 0; CurrentIteration < GJKIterationLimit; ++CurrentIteration)
	{
		Difference = FindClosestPoint(Result.Simplex, CoreA, CoreB);
		const float CoreDistance = Difference.Size();
		if (CoreDistance <= Epsilon)
		{
			Result.Status = ECollisionQueryStatus::Success;
			break;
		}
		const FVector Direction = -Difference / CoreDistance;
		const FSupportPoint Next = GetSupportPoint(A, B, Direction);
		// 현재 상한 거리와 support 분리 평면의 하한 거리 사이 간격.
		const double Gap = CoreDistance + FVector::Dot(Next.Difference, Direction);
		if (Gap <= Epsilon)
		{
			Result.Status = ECollisionQueryStatus::Success;
			break;
		}
		bool Duplicate = false;
		for (int32 CurrentSimplexPointIndex = 0; CurrentSimplexPointIndex < Result.Simplex.Count; ++CurrentSimplexPointIndex)
			if ((Next.Difference - Result.Simplex.Points[CurrentSimplexPointIndex].Difference).SizeSquared() <= Epsilon * Epsilon) Duplicate = true;
		if (Duplicate || Result.Simplex.Count == 4) break;
		Result.Simplex.Points[Result.Simplex.Count++] = Next;
	}
	Difference = FindClosestPoint(Result.Simplex, CoreA, CoreB);
	Result.CoreDistance = Difference.Size();
	Result.CorePointA = CoreA + A.Center;
	Result.CorePointB = CoreB + A.Center;
	Result.Normal = Difference.GetSafeNormal(FLT_MIN, (A.Center - B.Center).GetSafeNormal(FLT_MIN, FVector(1, 0, 0)));
	Result.PointA = Result.CorePointA - Result.Normal * A.GetMargin();
	Result.PointB = Result.CorePointB + Result.Normal * B.GetMargin();
	Result.Distance = std::max(0.0f, Result.CoreDistance - Margin);
	Result.bIntersecting = Result.Status == ECollisionQueryStatus::Success && Result.Distance <= Epsilon;
	return Result;
}

FPenetrationResult EPA::CalculatePenetration(const FCollisionWorldShape& A, const FCollisionWorldShape& B, const FGJKResult& GJKResult)
{
	FPenetrationResult Result;
	if (GJKResult.Status != ECollisionQueryStatus::Success)
	{
		Result.Status = GJKResult.Status;
		return Result;
	}
	const float Epsilon = CalculateTolerance(A, B);
	const float Margin = A.GetMargin() + B.GetMargin();
	if (!GJKResult.bIntersecting || GJKResult.CoreDistance > Epsilon)
	{
		Result.Status = ECollisionQueryStatus::Success;
		Result.Depth = std::max(0.0f, Margin - GJKResult.CoreDistance);
		Result.Normal = GJKResult.Normal;
		Result.PointA = GJKResult.PointA;
		Result.PointB = GJKResult.PointB;
		return Result;
	}

	TArray<FSupportPoint> Points;
	TArray<FVector> Directions;
	const auto SetBoundaryResult = [&](const FVector& Direction)
	{
		Result.Status = ECollisionQueryStatus::Success;
		Result.Depth = Margin;
		Result.Normal = -Direction;
		Result.PointA = GJKResult.CorePointA + Direction * A.GetMargin();
		Result.PointB = GJKResult.CorePointB - Direction * B.GetMargin();
	};
	for (int32 CurrentSimplexPointIndex = 0; CurrentSimplexPointIndex < GJKResult.Simplex.Count; ++CurrentSimplexPointIndex) Points.Add(GJKResult.Simplex.Points[CurrentSimplexPointIndex]);
	// 축·대각 방향과 simplex의 변·면에 수직인 방향으로 초기 다면체를 확장한다.
	for (int32 CurrentDirectionX = -1; CurrentDirectionX <= 1; ++CurrentDirectionX)
		for (int32 CurrentDirectionY = -1; CurrentDirectionY <= 1; ++CurrentDirectionY)
			for (int32 CurrentDirectionZ = -1; CurrentDirectionZ <= 1; ++CurrentDirectionZ)
				if (CurrentDirectionX || CurrentDirectionY || CurrentDirectionZ) Directions.Add(FVector(float(CurrentDirectionX), float(CurrentDirectionY), float(CurrentDirectionZ)).GetSafeNormal(FLT_MIN));
	for (int32 CurrentFirstPointIndex = 0; CurrentFirstPointIndex < GJKResult.Simplex.Count; ++CurrentFirstPointIndex)
		for (int32 CurrentSecondPointIndex = CurrentFirstPointIndex + 1; CurrentSecondPointIndex < GJKResult.Simplex.Count; ++CurrentSecondPointIndex)
		{
			const FVector Edge = Points[CurrentSecondPointIndex].Difference - Points[CurrentFirstPointIndex].Difference;
			for (int32 CurrentAxis = 0; CurrentAxis < 3; ++CurrentAxis)
			{
				FVector Basis(0);
				Basis[CurrentAxis] = 1;
				const FVector Normal = Edge.Cross(Basis);
				if (Normal.SizeSquared() > 0)
				{
					Directions.Add(Normal.GetSafeNormal(FLT_MIN));
					Directions.Add(-Normal.GetSafeNormal(FLT_MIN));
				}
			}
			for (int32 CurrentThirdPointIndex = CurrentSecondPointIndex + 1; CurrentThirdPointIndex < GJKResult.Simplex.Count; ++CurrentThirdPointIndex)
			{
				const FVector Normal = Edge.Cross(Points[CurrentThirdPointIndex].Difference - Points[CurrentFirstPointIndex].Difference);
				if (Normal.SizeSquared() > 0)
				{
					Directions.Add(Normal.GetSafeNormal(FLT_MIN));
					Directions.Add(-Normal.GetSafeNormal(FLT_MIN));
				}
			}
		}
	for (const FVector& CurrentDirection : Directions)
	{
		const FSupportPoint Point = GetSupportPoint(A, B, CurrentDirection);
		// 원점이 지지 평면 위라면 core는 접촉 또는 저차원이다. 침투 깊이는 margin이다.
		if (std::abs(FVector::Dot(Point.Difference, CurrentDirection)) <= Epsilon)
		{
			SetBoundaryResult(CurrentDirection);
			return Result;
		}
		if (!ContainsPoint(Points, Point, Epsilon)) Points.Add(Point);
	}

	// 원점을 엄격히 포함하는 사면체를 고른다. 경계 위의 simplex로 EPA를 시작하지 않는다.
	std::array<int32, 4> Seed{};
	double BestWeight = 0.0;
	for (int32 CurrentFirstPointIndex = 0; CurrentFirstPointIndex < Points.Num(); ++CurrentFirstPointIndex)
		for (int32 CurrentSecondPointIndex = CurrentFirstPointIndex + 1; CurrentSecondPointIndex < Points.Num(); ++CurrentSecondPointIndex)
			for (int32 CurrentThirdPointIndex = CurrentSecondPointIndex + 1; CurrentThirdPointIndex < Points.Num(); ++CurrentThirdPointIndex)
				for (int32 CurrentFourthPointIndex = CurrentThirdPointIndex + 1; CurrentFourthPointIndex < Points.Num(); ++CurrentFourthPointIndex)
				{
					int32 Indices[4] = {CurrentFirstPointIndex, CurrentSecondPointIndex, CurrentThirdPointIndex, CurrentFourthPointIndex};
					double Weights[4] = {};
					if (!ProjectOrigin(Points.GetData(), Indices, 4, Weights)) continue;
					const double Minimum = *std::min_element(std::begin(Weights), std::end(Weights));
					if (Minimum > BestWeight)
					{
						BestWeight = Minimum;
						Seed = {CurrentFirstPointIndex, CurrentSecondPointIndex, CurrentThirdPointIndex, CurrentFourthPointIndex};
					}
				}
	if (BestWeight <= BarycentricTolerance)
	{
		// 처음 simplex가 선분이면 임의 회전한 두 Capsule core의 평면 법선을 모를 수 있다.
		// 추가 support 점의 면 법선을 검사하여 실제 지지 평면이 원점을 지나는지 확인한다.
		for (int32 CurrentFirstPointIndex = 0; CurrentFirstPointIndex < Points.Num(); ++CurrentFirstPointIndex)
			for (int32 CurrentSecondPointIndex = CurrentFirstPointIndex + 1; CurrentSecondPointIndex < Points.Num(); ++CurrentSecondPointIndex)
				for (int32 CurrentThirdPointIndex = CurrentSecondPointIndex + 1; CurrentThirdPointIndex < Points.Num(); ++CurrentThirdPointIndex)
				{
					const FVector Normal = (Points[CurrentSecondPointIndex].Difference - Points[CurrentFirstPointIndex].Difference).Cross(Points[CurrentThirdPointIndex].Difference - Points[CurrentFirstPointIndex].Difference);
					if (Normal.SizeSquared() == 0.0f) continue;
					for (const FVector& CurrentDirection : {Normal.GetSafeNormal(FLT_MIN), -Normal.GetSafeNormal(FLT_MIN)})
						if (std::abs(FVector::Dot(GetSupportPoint(A, B, CurrentDirection).Difference, CurrentDirection)) <= Epsilon)
						{
							SetBoundaryResult(CurrentDirection);
							return Result;
						}
				}
		return Result;
	}
	Points = {Points[Seed[0]], Points[Seed[1]], Points[Seed[2]], Points[Seed[3]]};
	TArray<FEPAFace> Faces;
	for (const auto& CurrentFaceIndices : {std::array<int32, 3>{0, 1, 2}, {0, 3, 1}, {0, 2, 3}, {1, 3, 2}})
	{
		FEPAFace CurrentFace;
		if (!MakeFace(Points, CurrentFaceIndices[0], CurrentFaceIndices[1], CurrentFaceIndices[2], CurrentFace)) return Result;
		Faces.Add(CurrentFace);
	}
	for (int32 CurrentIteration = 0; CurrentIteration < EPAIterationLimit; ++CurrentIteration)
	{
		float NearestDistance = FLT_MAX;
		FVector NearestA, NearestB, Direction;
		for (const FEPAFace& CurrentFace : Faces)
		{
			FGJKSimplex Triangle;
			Triangle.Count = 3;
			Triangle.Points[0] = Points[CurrentFace.A];
			Triangle.Points[1] = Points[CurrentFace.B];
			Triangle.Points[2] = Points[CurrentFace.C];
			FVector CoreA, CoreB;
			const FVector ClosestPoint = FindClosestPoint(Triangle, CoreA, CoreB);
			const float Distance = ClosestPoint.Size();
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				NearestA = CoreA;
				NearestB = CoreB;
				Direction = ClosestPoint.GetSafeNormal(FLT_MIN, CurrentFace.Normal);
			}
		}
		const FSupportPoint Next = GetSupportPoint(A, B, Direction);
		if (FVector::Dot(Next.Difference, Direction) - NearestDistance <= Epsilon)
		{
			Result.Status = ECollisionQueryStatus::Success;
			Result.Depth = NearestDistance + Margin;
			Result.Normal = -Direction;
			Result.PointA = NearestA + A.Center + Direction * A.GetMargin();
			Result.PointB = NearestB + A.Center - Direction * B.GetMargin();
			return Result;
		}
		if (ContainsPoint(Points, Next, Epsilon)) return Result;
		TArray<std::pair<int32, int32>> Edges;
		TArray<FEPAFace> RemainingFaces;
		for (const FEPAFace& CurrentFace : Faces)
		{
			if (FVector::Dot(CurrentFace.Normal, Next.Difference - Points[CurrentFace.A].Difference) <= 0.0)
			{
				RemainingFaces.Add(CurrentFace);
				continue;
			}
			for (auto CurrentEdge : {std::pair{CurrentFace.A, CurrentFace.B}, {CurrentFace.B, CurrentFace.C}, {CurrentFace.C, CurrentFace.A}})
			{
				const int32 ReverseEdgeIndex = Edges.Find(std::pair{CurrentEdge.second, CurrentEdge.first});
				if (ReverseEdgeIndex < Edges.Num()) Edges.RemoveAt(ReverseEdgeIndex);
				else Edges.Add(CurrentEdge);
			}
		}
		if (Edges.IsEmpty()) return Result;
		const int32 NewPointIndex = Points.Num();
		Points.Add(Next);
		for (const auto& CurrentEdge : Edges)
		{
			FEPAFace CurrentFace;
			if (!MakeFace(Points, CurrentEdge.first, CurrentEdge.second, NewPointIndex, CurrentFace)) return Result;
			RemainingFaces.Add(CurrentFace);
		}
		Faces = std::move(RemainingFaces);
	}
	return Result;
}

FShapeSweepResult GJK::Sweep(const FCollisionWorldShape& A, const FCollisionWorldShape& B, const FVector& Delta)
{
	FShapeSweepResult Result;
	const float Epsilon = CalculateTolerance(A, B);
	double Time = 0.0;
	FVector LastNormal;
	FCollisionWorldShape Moving = A;
	for (int32 CurrentIteration = 0; CurrentIteration < SweepIterationLimit; ++CurrentIteration)
	{
		Moving.Center = A.Center + Delta * static_cast<float>(Time);
		const FGJKResult DistanceResult = CalculateDistance(Moving, B);
		if (DistanceResult.Status != ECollisionQueryStatus::Success)
		{
			Result.Status = DistanceResult.Status;
			return Result;
		}
		if (DistanceResult.bIntersecting)
		{
			Result.Time = static_cast<float>(Time);
			Result.Normal = DistanceResult.Normal;
			if (Time > 0.0 && DistanceResult.CoreDistance <= Epsilon)
				Result.Normal = LastNormal;
			Result.ImpactPoint = DistanceResult.PointB;
			if (Time == 0.0)
			{
				const FPenetrationResult Penetration = EPA::CalculatePenetration(A, B, DistanceResult);
				if (Penetration.Status != ECollisionQueryStatus::Success)
				{
					Result.Status = Penetration.Status;
					return Result;
				}
				Result.PenetrationDepth = Penetration.Depth;
				Result.bStartPenetrating = Penetration.Depth > Epsilon;
				Result.Normal = Penetration.Normal;
				Result.ImpactPoint = Penetration.PointB;
			}
			Result.Status = ECollisionQueryStatus::Success;
			Result.bHit = true;
			return Result;
		}
		const double ClosingSpeed = -FVector::Dot(Delta, DistanceResult.Normal);
		if (ClosingSpeed <= 0.0)
		{
			Result.Status = ECollisionQueryStatus::Success;
			return Result;
		}
		// GJK 거리 오차만큼 여유를 두어 분리 평면을 넘어가지 않는다.
		LastNormal = DistanceResult.Normal;
		const double NextTime = Time + std::max(0.0f, DistanceResult.Distance - Epsilon) / ClosingSpeed;
		if (NextTime > 1.0)
		{
			Result.Status = ECollisionQueryStatus::Success;
			return Result;
		}
		if (NextTime <= Time) return Result;
		Time = NextTime;
	}
	return Result;
}
