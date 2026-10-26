#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

class UTextRenderComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
class UCameraComponent;
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
	void HandleCountdownChanged(const FString& NewCountdown);

	float GetMaxSpeed() const { return MaxSpeed; }

	UTextRenderComponent* ScoreText = nullptr;

	UTextRenderComponent* CountdownText = nullptr;

	UTextRenderComponent* RaceTimeText = nullptr;

	USceneComponent* SpeedNeedlePivot = nullptr;
protected:
	void UpdateHudForFieldOfView();

	void UpdateRaceTimeText();
	FString LastRaceTimeLabel;

	UCameraComponent* VehicleCamera = nullptr;
	UStaticMeshComponent* SpeedDial = nullptr;

	float BaseFieldOfView = 0.0f;
	float LastAppliedFieldOfView = -1.0f;

	float MaxSpeed = 6.0f;

private:
	void HandleComponentHit(UPrimitiveComponent* HitComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit);
};
