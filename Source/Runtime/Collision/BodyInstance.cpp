#include "EnginePCH.h"
#include "BodyInstance.h"
#include "PhysicsScene.h"
#include "Component/PrimitiveComponent.h"

void FBodyInstance::InitBody(const UPrimitiveComponent* InOwner)
{
	OwnerComponent = InOwner;
	UpdateTransform();
}

void FBodyInstance::UpdateTransform()
{
	WorldShape = FCollisionWorldShape{};
	Bounds = FBox{};
	if (!OwnerComponent) return;

	const FCollisionLocalShape LocalShape = OwnerComponent->GetCollisionShape();
	if (LocalShape.Type == ECollisionShapeType::None) return;

	const FTransform WorldTransform(OwnerComponent->GetWorldRotation(), OwnerComponent->GetWorldLocation(), OwnerComponent->GetWorldScale3D());
	WorldShape = FCollisionWorldShape::FromLocal(LocalShape, WorldTransform);
	Bounds = WorldShape.CalcBounds();
}

void FBodyInstance::TerminateBody()
{
	if (PhysicsScene)
		PhysicsScene->RemoveBody(this);
	OwnerComponent = nullptr;
	WorldShape = FCollisionWorldShape{};
	Bounds = FBox{};
}
