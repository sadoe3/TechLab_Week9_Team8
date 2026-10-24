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

	const ECollisionShapeType Type = Body->GetWorldShape().Type;
	if (Type != ECollisionShapeType::Box && Type != ECollisionShapeType::Sphere && Type != ECollisionShapeType::Capsule) return;

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
