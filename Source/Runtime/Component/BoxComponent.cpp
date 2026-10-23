#include "EnginePCH.h"
#include "BoxComponent.h"
#include "Render/LineBatcher.h"

void UBoxComponent::SetBoxExtent(const FVector& InExtent)
{
	BoxExtent = FVector(std::max(0.0f, InExtent.X), std::max(0.0f, InExtent.Y), std::max(0.0f, InExtent.Z));
	MarkTransformDirty();
}

FBox UBoxComponent::CalcLocalBounds() const
{
	return FBox{ -BoxExtent, BoxExtent };
}

void UBoxComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	const FLinearColor Color = ShapeColor.ReinterpretAsLinear();
	LineBatcher->AddOrientedBox(CalcLocalBounds(), GetWorldMatrix(), FVector4(Color.R, Color.G, Color.B, Color.A));
}

void UBoxComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "BoxExtent")
		SetBoxExtent(BoxExtent);
}
