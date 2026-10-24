#pragma once

#include "ShapeComponent.h"

class UBoxComponent : public UShapeComponent
{
	DECLARE_CLASS(UBoxComponent, UShapeComponent)
	REFLECT_START(UBoxComponent)
		PROPERTY(BoxExtent)
	REFLECT_END()

public:
	const FVector& GetBoxExtent() const { return BoxExtent; }
	void SetBoxExtent(const FVector& InExtent);
	virtual FCollisionLocalShape GetCollisionShape() const override;
	virtual FBox CalcLocalBounds() const override;
	virtual void DrawDebug(FLineBatcher* LineBatcher) const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;

protected:
	// 배율 적용 전, 중심에서 각 면까지의 반 길이.
	FVector BoxExtent = FVector(0.5f, 0.5f, 0.5f);
};
