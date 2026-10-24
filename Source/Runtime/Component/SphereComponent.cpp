#include "EnginePCH.h"
#include "SphereComponent.h"
#include "Render/LineBatcher.h"

void USphereComponent::SetSphereRadius(float InRadius)
{
	SphereRadius = std::max(0.0f, InRadius);
	MarkTransformDirty();
}

FBox USphereComponent::CalcLocalBounds() const
{
	const FVector Extent(SphereRadius);
	return FBox{ -Extent, Extent };
}

FCollisionLocalShape USphereComponent::GetCollisionShape() const
{
	FCollisionLocalShape Shape;
	Shape.Type = ECollisionShapeType::Sphere;
	Shape.Radius = SphereRadius;
	return Shape;
}

void USphereComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	const FLinearColor Color = ShapeColor.ReinterpretAsLinear();
	const FCollisionWorldShape& Shape = GetBodyInstance().GetWorldShape();
	LineBatcher->AddSphere(Shape.Center, Shape.Radius, FVector4(Color.R, Color.G, Color.B, Color.A));
}

void USphereComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "SphereRadius")
		SetSphereRadius(SphereRadius);
}
