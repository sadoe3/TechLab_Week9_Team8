#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

class ARacingCar : public ACharacter
{
	DECLARE_CLASS(ARacingCar, ACharacter)

public:
	ARacingCar();
	virtual void BeginPlay() override;
};
