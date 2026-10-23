#pragma once

#include "ShapeComponent.h"

class USphereComponent : public UShapeComponent
{
	DECLARE_CLASS(USphereComponent, UShapeComponent)
	REFLECT_START(USphereComponent)
		PROPERTY(SphereRadius)
	REFLECT_END()

public:
	float GetSphereRadius() const { return SphereRadius; }
	float GetScaledSphereRadius() const;
	void SetSphereRadius(float InRadius);
	virtual FBox CalcLocalBounds() const override;
	virtual FBox CalcBounds() const override;
	virtual void DrawDebug(FLineBatcher* LineBatcher) const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;

protected:
	float SphereRadius = 0.5f;
};
