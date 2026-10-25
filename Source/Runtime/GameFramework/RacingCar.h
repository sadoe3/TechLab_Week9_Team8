#pragma once

#include "GameFramework/Actor.h"

class ARacingCar : public AActor
{
	DECLARE_CLASS(ARacingCar, AActor)

public:
	ARacingCar();
	virtual void BeginPlay() override;
};
