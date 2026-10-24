#include "EnginePCH.h"
#include "BoxComponent.h"
#include "Render/LineBatcher.h"

void UBoxComponent::SetBoxExtent(const FVector& InExtent)
{
	BoxExtent = FVector(std::max(0.0f, InExtent.X), std::max(0.0f, InExtent.Y), std::max(0.0f, InExtent.Z));
	MarkTransformDirty();
}

FCollisionLocalShape UBoxComponent::GetCollisionShape() const
{
	FCollisionLocalShape Shape;
	Shape.Type = ECollisionShapeType::Box;
	Shape.BoxExtent = BoxExtent;
	return Shape;
}

FBox UBoxComponent::CalcLocalBounds() const
{
	return FBox{ -BoxExtent, BoxExtent };
}

void UBoxComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	const FLinearColor Color = ShapeColor.ReinterpretAsLinear();
	const FCollisionWorldShape& Shape = GetBodyInstance().GetWorldShape();
	LineBatcher->AddOrientedBox(FBox{ -Shape.BoxExtent, Shape.BoxExtent }, FTransform(Shape.Rotation.ToFRotator(), Shape.Center).GetLocalMatrix(), FVector4(Color.R, Color.G, Color.B, Color.A));
}

void UBoxComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "BoxExtent")
		SetBoxExtent(BoxExtent);
}
