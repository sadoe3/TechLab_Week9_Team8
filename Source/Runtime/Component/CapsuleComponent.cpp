#include "EnginePCH.h"
#include "CapsuleComponent.h"
#include "Render/LineBatcher.h"

void UCapsuleComponent::SetCapsuleSize(float InRadius, float InHalfHeight)
{
	CapsuleRadius = std::max(0.0f, InRadius);
	CapsuleHalfHeight = std::max(CapsuleRadius, InHalfHeight);
	MarkTransformDirty();
}

FBox UCapsuleComponent::CalcLocalBounds() const
{
	const FVector Extent(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight);
	return FBox{ -Extent, Extent };
}

FCollisionLocalShape UCapsuleComponent::GetCollisionShape() const
{
	FCollisionLocalShape Shape;
	Shape.Type = ECollisionShapeType::Capsule;
	Shape.Radius = CapsuleRadius;
	Shape.HalfHeight = CapsuleHalfHeight;
	return Shape;
}

void UCapsuleComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	const FCollisionWorldShape& Shape = GetBodyInstance().GetWorldShape();
	const FLinearColor Color = ShapeColor.ReinterpretAsLinear();
	LineBatcher->AddCapsule(Shape.Center, Shape.Radius, Shape.HalfHeight, Shape.Rotation, FVector4(Color.R, Color.G, Color.B, Color.A));
}

void UCapsuleComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "CapsuleRadius" || Property.Name == "CapsuleHalfHeight")
		SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
}
