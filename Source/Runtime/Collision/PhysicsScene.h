#pragma once

#include "Containers/Array.h"

class FBodyInstance;

class FPhysicsScene
{
public:
	FPhysicsScene() = default;
	~FPhysicsScene();
	FPhysicsScene(const FPhysicsScene&) = delete;
	FPhysicsScene& operator=(const FPhysicsScene&) = delete;

	void AddBody(FBodyInstance* Body);
	void RemoveBody(FBodyInstance* Body);
	void RemoveAllBodies();
	const TArray<FBodyInstance*>& GetBodies() const { return Bodies; }

private:
	TArray<FBodyInstance*> Bodies;
};
