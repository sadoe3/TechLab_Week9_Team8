#include "EnginePCH.h"
#include "CollisionShape.h"

FCollisionWorldShape FCollisionWorldShape::FromLocal(const FCollisionLocalShape& LocalShape, const FTransform& WorldTransform)
{
	FCollisionWorldShape Result;
	Result.Type = LocalShape.Type;
	if (LocalShape.Type == ECollisionShapeType::None) return Result;

	Result.Center = WorldTransform.Location;
	Result.Rotation = WorldTransform.GetOrientation();
	const FVector Scale(std::abs(WorldTransform.Scale.X), std::abs(WorldTransform.Scale.Y), std::abs(WorldTransform.Scale.Z));
	switch (LocalShape.Type)
	{
	case ECollisionShapeType::Box:
		Result.BoxExtent = LocalShape.BoxExtent * Scale;
		break;
	case ECollisionShapeType::Sphere:
		Result.Radius = LocalShape.Radius * std::min({ Scale.X, Scale.Y, Scale.Z });
		break;
	case ECollisionShapeType::Capsule:
		Result.HalfHeight = LocalShape.HalfHeight * Scale.Z;
		Result.Radius = std::min(LocalShape.Radius * std::max(Scale.X, Scale.Y), Result.HalfHeight);
		break;
	}
	return Result;
}

FBox FCollisionWorldShape::CalcBounds() const
{
	if (Type == ECollisionShapeType::None) return FBox{};

	if (Type == ECollisionShapeType::Box)
		return FBox{ -BoxExtent, BoxExtent }.GetWorldAABB(FTransform(Rotation.ToFRotator(), Center).GetLocalMatrix());

	FVector Extent(Radius);
	if (Type == ECollisionShapeType::Capsule)
	{
		const FVector Segment = Rotation.RotateVector(FVector(0, 0, 1)) * (HalfHeight - Radius);
		Extent += FVector(std::abs(Segment.X), std::abs(Segment.Y), std::abs(Segment.Z));
	}
	return FBox{ Center - Extent, Center + Extent };
}
