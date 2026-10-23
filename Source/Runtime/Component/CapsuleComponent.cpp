#include "EnginePCH.h"
#include "CapsuleComponent.h"
#include "Render/LineBatcher.h"

void UCapsuleComponent::SetCapsuleSize(float InRadius, float InHalfHeight)
{
	CapsuleRadius = std::max(0.0f, InRadius);
	CapsuleHalfHeight = std::max(CapsuleRadius, InHalfHeight);
	MarkTransformDirty();
}

void UCapsuleComponent::GetScaledCapsuleSize(float& OutRadius, float& OutHalfHeight) const
{
	const FVector Scale = GetWorldScale3D();
	OutHalfHeight = CapsuleHalfHeight * std::abs(Scale.Z);
	OutRadius = std::min(CapsuleRadius * std::max(std::abs(Scale.X), std::abs(Scale.Y)), OutHalfHeight);
}

FBox UCapsuleComponent::CalcLocalBounds() const
{
	const FVector Extent(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight);
	return FBox{ -Extent, Extent };
}

FBox UCapsuleComponent::CalcBounds() const
{
	float Radius, HalfHeight;
	GetScaledCapsuleSize(Radius, HalfHeight);
	const FVector Axis = GetWorldRotation().Quaternion().RotateVector(FVector(0, 0, 1));
	const FVector Segment = Axis * (HalfHeight - Radius);
	const FVector Extent = FVector(std::abs(Segment.X), std::abs(Segment.Y), std::abs(Segment.Z)) + FVector(Radius);
	const FVector Center = GetWorldLocation();
	return FBox{ Center - Extent, Center + Extent };
}

void UCapsuleComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	float Radius, HalfHeight;
	GetScaledCapsuleSize(Radius, HalfHeight);
	const FLinearColor Color = ShapeColor.ReinterpretAsLinear();
	LineBatcher->AddCapsule(GetWorldLocation(), Radius, HalfHeight, GetWorldRotation().Quaternion(), FVector4(Color.R, Color.G, Color.B, Color.A));
}

void UCapsuleComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "CapsuleRadius" || Property.Name == "CapsuleHalfHeight")
		SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
}
