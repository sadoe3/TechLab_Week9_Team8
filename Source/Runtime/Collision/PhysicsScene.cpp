#include "EnginePCH.h"
#include "PhysicsScene.h"
#include "BodyInstance.h"

FPhysicsScene::~FPhysicsScene()
{
	RemoveAllBodies();
}

void FPhysicsScene::AddBody(FBodyInstance* Body)
{
	if (!Body || Body->PhysicsScene == this) return;

	const ECollisionShapeType Type = Body->GetWorldShape().Type;
	if (Type != ECollisionShapeType::Box && Type != ECollisionShapeType::Sphere && Type != ECollisionShapeType::Capsule) return;

	if (Body->PhysicsScene)
		Body->PhysicsScene->RemoveBody(Body);
	Bodies.Add(Body);
	Body->PhysicsScene = this;
}

void FPhysicsScene::RemoveBody(FBodyInstance* Body)
{
	if (!Body || Body->PhysicsScene != this) return;

	const int32 Index = Bodies.Find(Body);
	Bodies.RemoveAtSwap(Index);
	Body->PhysicsScene = nullptr;
}

void FPhysicsScene::RemoveAllBodies()
{
	for (FBodyInstance* Body : Bodies)
		Body->PhysicsScene = nullptr;
	Bodies.Reset();
}
