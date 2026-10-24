#include "EnginePCH.h"
#include "BodyInstance.h"
#include "PhysicsScene.h"
#include "Component/PrimitiveComponent.h"

void FBodyInstance::InitBody(const UPrimitiveComponent* InOwner)
{
	OwnerComponent = InOwner;
	UpdateFilter();
	UpdateTransform();
}

void FBodyInstance::UpdateFilter()
{
	FilterData = {};
	bQueryEnabled = false;
	if (!OwnerComponent) return;

	FilterData = FCollisionFilterData::FromSettings(OwnerComponent->GetCollisionObjectType(), OwnerComponent->GetCollisionResponses(), OwnerComponent->GetOwner());
	bQueryEnabled = OwnerComponent->GetCollisionEnabled() == ECollisionEnabled::QueryOnly;
}

void FBodyInstance::UpdateTransform()
{
	WorldShape = FCollisionWorldShape{};
	Bounds = FBox{};
	if (!OwnerComponent) return;

	const FCollisionLocalShape LocalShape = OwnerComponent->GetCollisionShape();
	if (LocalShape.Type == ECollisionShapeType::None) return;

	const FTransform WorldTransform(OwnerComponent->GetWorldRotation(), OwnerComponent->GetWorldLocation(), OwnerComponent->GetWorldScale3D());
	WorldShape = FCollisionWorldShape::CreateFromLocal(LocalShape, WorldTransform);
	Bounds = WorldShape.CalcBounds();
	if (PhysicsScene)
		PhysicsScene->UpdateBody(this);
}

void FBodyInstance::TerminateBody()
{
	if (PhysicsScene)
		PhysicsScene->RemoveBody(this);
	OwnerComponent = nullptr;
	WorldShape = FCollisionWorldShape{};
	Bounds = FBox{};
	FilterData = {};
	bQueryEnabled = false;
}
