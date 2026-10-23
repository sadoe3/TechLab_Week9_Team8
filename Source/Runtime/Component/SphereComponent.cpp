#include "EnginePCH.h"
#include "SphereComponent.h"
#include "Render/LineBatcher.h"

void USphereComponent::SetSphereRadius(float InRadius)
{
	SphereRadius = std::max(0.0f, InRadius);
	MarkTransformDirty();
}

float USphereComponent::GetScaledSphereRadius() const
{
	const FVector Scale = GetWorldScale3D();
	return SphereRadius * std::min({ std::abs(Scale.X), std::abs(Scale.Y), std::abs(Scale.Z) });
}

FBox USphereComponent::CalcLocalBounds() const
{
	const FVector Extent(SphereRadius);
	return FBox{ -Extent, Extent };
}

FBox USphereComponent::CalcBounds() const
{
	const FVector Center = GetWorldLocation();
	const FVector Extent(GetScaledSphereRadius());
	return FBox{ Center - Extent, Center + Extent };
}

void USphereComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	const FLinearColor Color = ShapeColor.ReinterpretAsLinear();
	LineBatcher->AddSphere(GetWorldLocation(), GetScaledSphereRadius(), FVector4(Color.R, Color.G, Color.B, Color.A));
}

void USphereComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "SphereRadius")
		SetSphereRadius(SphereRadius);
}
