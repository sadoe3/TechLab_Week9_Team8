#include "EnginePCH.h"
#include "PhysicsScene.h"
#include "BodyInstance.h"

FPhysicsScene::FPhysicsScene() : BVH([](FBodyInstance* const& Body) { return Body->GetBounds(); })
{
}

FPhysicsScene::~FPhysicsScene()
{
	RemoveAllBodies();
}

void FPhysicsScene::AddBody(FBodyInstance* Body)
{
	if (!Body || Body->PhysicsScene == this) return;
	if (!Body->IsQueryEnabled()) return;

	if (!Body->GetWorldShape().IsSupported()) return;

	if (Body->PhysicsScene)
		Body->PhysicsScene->RemoveBody(Body);
	Bodies.Add(Body);
	Body->PhysicsScene = this;
	bBodiesChanged = true;
}

void FPhysicsScene::UpdateBody(FBodyInstance* Body)
{
	if (Body && Body->PhysicsScene == this)
		bBoundsChanged = true;
}

void FPhysicsScene::RemoveBody(FBodyInstance* Body)
{
	if (!Body || Body->PhysicsScene != this) return;

	const int32 Index = Bodies.Find(Body);
	Bodies.RemoveAtSwap(Index);
	Body->PhysicsScene = nullptr;
	bBodiesChanged = true;
}

void FPhysicsScene::RemoveAllBodies()
{
	for (FBodyInstance* Body : Bodies)
		Body->PhysicsScene = nullptr;
	Bodies.Reset();
	BVH.Clear();
	bBodiesChanged = false;
	bBoundsChanged = false;
}

void FPhysicsScene::QueryCandidates(const FBox& Bounds, const FCollisionQueryParams& Params, TArray<FCollisionCandidate>& OutCandidates)
{
	OutCandidates.Reset();
	// 삭제된 Body가 예전 BVH에 남아 있어도 Refit/Query 전에 현재 목록으로 재구성한다.
	if (bBodiesChanged)
		BVH.Build(std::span(Bodies.GetData(), Bodies.Num()));
	else if (bBoundsChanged)
		BVH.Refit();
	bBodiesChanged = false;
	bBoundsChanged = false;

	const FCollisionFilterData QueryFilter = FCollisionFilterData::FromSettings(Params.Channel, Params.Responses, Params.IgnoredOwner);
	BVH.Query([&Bounds](const FBox& CandidateBounds)
	{
		return Bounds.Min.X <= CandidateBounds.Max.X && Bounds.Max.X >= CandidateBounds.Min.X
			&& Bounds.Min.Y <= CandidateBounds.Max.Y && Bounds.Max.Y >= CandidateBounds.Min.Y
			&& Bounds.Min.Z <= CandidateBounds.Max.Z && Bounds.Max.Z >= CandidateBounds.Min.Z;
	}, [&](FBodyInstance* Body)
	{
		const FCollisionFilterData& Filter = Body->GetFilterData();
		if (Params.IgnoredOwner && Filter.Owner == Params.IgnoredOwner) return;
		const ECollisionResponse Response = QueryFilter.GetResponseTo(Filter);
		if (Response == ECollisionResponse::Ignore || (Params.bIgnoreBlocks && Response == ECollisionResponse::Block)) return;
		OutCandidates.Add({ Body, Response });
	});
}

ECollisionQueryStatus FPhysicsScene::QueryOverlapMulti(const FCollisionWorldShape& Shape, const FCollisionQueryParams& Params, TArray<FOverlapResult>& OutOverlaps)
{
	OutOverlaps.Reset();
	if (!Shape.IsSupported()) return ECollisionQueryStatus::InvalidShape;

	TArray<FCollisionCandidate> Candidates;
	QueryCandidates(Shape.CalcBounds(), Params, Candidates);
	for (const FCollisionCandidate& Candidate : Candidates)
	{
		const FGJKResult Result = GJK::CalculateDistance(Shape, Candidate.Body->GetWorldShape());
		if (Result.Status != ECollisionQueryStatus::Success)
		{
			OutOverlaps.Reset();
			return Result.Status;
		}
		if (Result.bIntersecting)
			OutOverlaps.Add({const_cast<UPrimitiveComponent*>(Candidate.Body->GetOwnerComponent()), Candidate.Response == ECollisionResponse::Block});
	}
	return ECollisionQueryStatus::Success;
}

ECollisionQueryStatus FPhysicsScene::QuerySweepMulti(const FCollisionWorldShape& Shape, const FVector& Delta, const FCollisionQueryParams& Params, TArray<FHitResult>& OutHits)
{
	OutHits.Reset();
	if (!Shape.IsSupported()) return ECollisionQueryStatus::InvalidShape;

	FCollisionWorldShape EndShape = Shape;
	EndShape.Center += Delta;
	FBox Bounds = Shape.CalcBounds();
	Bounds.Expand(EndShape.CalcBounds());
	TArray<FCollisionCandidate> Candidates;
	QueryCandidates(Bounds, Params, Candidates);
	float Limit = 1.0f;
	for (const FCollisionCandidate& Candidate : Candidates)
	{
		const FShapeSweepResult Result = GJK::Sweep(Shape, Candidate.Body->GetWorldShape(), Delta * Limit);
		if (Result.Status != ECollisionQueryStatus::Success)
		{
			OutHits.Reset();
			return Result.Status;
		}
		if (!Result.bHit) continue;
		if (Params.bIgnoreInitialBlockingHitsWhenMovingOut && Candidate.Response == ECollisionResponse::Block && Result.Time == 0.0f && FVector::Dot(Delta, Result.Normal) > 0.0)
			continue;
		FHitResult Hit;
		Hit.Time = Result.Time * Limit;
		Hit.Distance = Delta.Size() * Hit.Time;
		Hit.Location = Shape.Center + Delta * Hit.Time;
		Hit.ImpactPoint = Result.ImpactPoint;
		Hit.Normal = Hit.ImpactNormal = Result.Normal;
		Hit.TraceStart = Shape.Center;
		Hit.TraceEnd = EndShape.Center;
		Hit.PenetrationDepth = Result.PenetrationDepth;
		Hit.bStartPenetrating = Result.bStartPenetrating;
		Hit.bBlockingHit = Candidate.Response == ECollisionResponse::Block;
		Hit.HitComponent = const_cast<UPrimitiveComponent*>(Candidate.Body->GetOwnerComponent());
		OutHits.Add(Hit);
		if (Hit.bBlockingHit) 
			Limit = Hit.Time;
	}
	// 같은 시각의 Overlap을 먼저 두고 가장 이른 Block 하나만 마지막에 남긴다.
	if (OutHits.Num() > 1)
		std::stable_sort(OutHits.begin(), OutHits.end(), [](const FHitResult& A, const FHitResult& B)
		{
			if (A.Time != B.Time) return A.Time < B.Time;
			return !A.bBlockingHit && B.bBlockingHit;
		});
	for (int32 Index = 0; Index < OutHits.Num(); ++Index)
		if (OutHits[Index].bBlockingHit)
		{
			OutHits.SetNum(Index + 1);
			break;
		}
	return ECollisionQueryStatus::Success;
}
