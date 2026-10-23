#pragma once

#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent
{
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
	REFLECT_START(UCapsuleComponent)
		PROPERTY(CapsuleRadius)
		PROPERTY(CapsuleHalfHeight)
	REFLECT_END()

public:
	float GetCapsuleRadius() const { return CapsuleRadius; }
	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
	void SetCapsuleSize(float InRadius, float InHalfHeight);
	void GetScaledCapsuleSize(float& OutRadius, float& OutHalfHeight) const;
	virtual FBox CalcLocalBounds() const override;
	virtual FBox CalcBounds() const override;
	virtual void DrawDebug(FLineBatcher* LineBatcher) const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;

protected:
	float CapsuleRadius = 0.5f;
	// local Z축 방향이며 중심에서 반구 끝까지 포함한다.
	float CapsuleHalfHeight = 1.0f;
};
