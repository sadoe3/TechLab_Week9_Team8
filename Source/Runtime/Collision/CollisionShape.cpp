#include "EnginePCH.h"
#include "CollisionShape.h"

bool FCollisionWorldShape::IsSupported() const
{
	return Type == ECollisionShapeType::Box || Type == ECollisionShapeType::Sphere || Type == ECollisionShapeType::Capsule;
}

float FCollisionWorldShape::GetMargin() const
{
	return Type == ECollisionShapeType::Sphere || Type == ECollisionShapeType::Capsule ? Radius : 0.0f;
}

FVector FCollisionWorldShape::GetCoreSupportPoint(const FVector& Direction) const
{
	if (Type == ECollisionShapeType::Box)
	{
		FVector Result = Center;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			FVector LocalAxis(0);
			LocalAxis[Axis] = 1.0f;
			const FVector WorldAxis = Rotation.RotateVector(LocalAxis);
			Result += WorldAxis * (Direction.Dot(WorldAxis) >= 0.0f ? BoxExtent[Axis] : -BoxExtent[Axis]);
		}
		return Result;
	}
	if (Type == ECollisionShapeType::Capsule)
	{
		const FVector Axis = Rotation.RotateVector(FVector(0, 0, 1));
		return Center + Axis * ((Direction.Dot(Axis) >= 0.0f ? 1.0f : -1.0f) * (HalfHeight - Radius));
	}
	return Center;
}

FVector FCollisionWorldShape::GetSupportPoint(const FVector& Direction) const
{
	const float Length = Direction.Size();
	const FVector UnitDirection = Length > 0.0f ? Direction / Length : FVector(1, 0, 0);
	return GetCoreSupportPoint(UnitDirection) + UnitDirection * GetMargin();
}

FCollisionWorldShape FCollisionWorldShape::CreateFromLocal(const FCollisionLocalShape& LocalShape, const FTransform& WorldTransform)
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
