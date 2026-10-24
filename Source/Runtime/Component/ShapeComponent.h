#pragma once

#include "PrimitiveComponent.h"

class FLineBatcher;

class UShapeComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)
	REFLECT_START(UShapeComponent)
		PROPERTY(ShapeColor)
		PROPERTY(bDrawOnlyIfSelected)
	REFLECT_END()

public:
	UShapeComponent()
	{
		bCastShadow = false;
		bReceiveShadow = false;
		CollisionEnabled = static_cast<int32>(ECollisionEnabled::QueryOnly);
	}

	const FColor& GetShapeColor() const { return ShapeColor; }
	void SetShapeColor(const FColor& InColor) { ShapeColor = InColor; }
	bool ShouldDrawOnlyIfSelected() const { return bDrawOnlyIfSelected; }
	void SetDrawOnlyIfSelected(bool bValue) { bDrawOnlyIfSelected = bValue; }
	virtual void DrawDebug(FLineBatcher* LineBatcher) const = 0;
	virtual FBox CalcBounds() const override { return GetBodyInstance().GetBounds(); }

protected:
	FColor ShapeColor = FColor(0, 255, 0);
	bool bDrawOnlyIfSelected = false;
};
