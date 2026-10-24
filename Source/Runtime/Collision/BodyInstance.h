#pragma once

#include "CollisionShape.h"

class UPrimitiveComponent;
class FPhysicsScene;

class FBodyInstance
{
public:
	void InitBody(const UPrimitiveComponent* InOwner);
	void UpdateTransform();
	void TerminateBody();
	const UPrimitiveComponent* GetOwnerComponent() const { return OwnerComponent; }
	const FCollisionWorldShape& GetWorldShape() const { return WorldShape; }
	const FBox& GetBounds() const { return Bounds; }
	FPhysicsScene* GetPhysicsScene() const { return PhysicsScene; }

private:
	friend class FPhysicsScene;
	FPhysicsScene* PhysicsScene = nullptr;
	const UPrimitiveComponent* OwnerComponent = nullptr;
	FCollisionWorldShape WorldShape;		// todo: 추후 mesh component 쪽 처리를 할때 여러 world shape들의 모음으로 하나의 body를 구성하도록 개선시킬 수 있다.
	FBox Bounds{};
};
