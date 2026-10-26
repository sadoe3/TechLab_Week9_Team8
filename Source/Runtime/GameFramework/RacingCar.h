#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

class UTextRenderComponent;

class ARacingCar : public ACharacter
{
	DECLARE_CLASS(ARacingCar, ACharacter)
	virtual void Tick(float DeltaTime) override;

public:
	ARacingCar();
	virtual void BeginPlay() override;
	UTextRenderComponent* ScoreText = nullptr;
};
