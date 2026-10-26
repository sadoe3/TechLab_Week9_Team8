#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

class UPrimitiveComponent;
struct FHitResult;

class ARacingCar : public ACharacter
{
	DECLARE_CLASS(ARacingCar, ACharacter)
	REFLECT_START(ARacingCar)
		PROPERTY(MaxSpeed)
	REFLECT_END()

public:
	ARacingCar();
	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	float GetMaxSpeed() const { return MaxSpeed; }

protected:
	float MaxSpeed = 6.0f;

private:
	void HandleComponentHit(UPrimitiveComponent* HitComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit);
};
