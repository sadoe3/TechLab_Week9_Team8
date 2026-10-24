#pragma once

#include "Containers/Array.h"
#include "Math/BVH.h"
#include "CollisionFilter.h"

class FBodyInstance;

struct FCollisionCandidate
{
	const FBodyInstance* Body;
	ECollisionResponse Response;
};

class FPhysicsScene
{
public:
	FPhysicsScene();
	~FPhysicsScene();
	FPhysicsScene(const FPhysicsScene&) = delete;
	FPhysicsScene& operator=(const FPhysicsScene&) = delete;

	void AddBody(FBodyInstance* Body);
	void UpdateBody(FBodyInstance* Body);
	void RemoveBody(FBodyInstance* Body);
	void RemoveAllBodies();
	const TArray<FBodyInstance*>& GetBodies() const { return Bodies; }
	// AABB와 필터로 후보만 반환한다. 실제 도형 접촉 판정은 포함하지 않는다.
	void QueryCandidates(const FBox& Bounds, const FCollisionQueryParams& Params, TArray<FCollisionCandidate>& OutCandidates);

private:
	TArray<FBodyInstance*> Bodies;
	TBVH<FBodyInstance*> BVH;
	bool bBodiesChanged = false;
	bool bBoundsChanged = false;
};
