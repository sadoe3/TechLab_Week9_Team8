#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

class UTextRenderComponent;
class UPrimitiveComponent;
class USceneComponent;
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
	virtual void Tick(float DeltaTime) override;

	void HandleScoreChanged(int32 NewScore);

	float GetMaxSpeed() const { return MaxSpeed; }

	UTextRenderComponent* ScoreText = nullptr;

	USceneComponent* SpeedNeedlePivot = nullptr;
protected:
	float MaxSpeed = 6.0f;

private:
	void HandleComponentHit(UPrimitiveComponent* HitComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit);
};
