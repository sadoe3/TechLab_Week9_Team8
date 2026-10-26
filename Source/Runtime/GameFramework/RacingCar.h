#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

class UTextRenderComponent;

class ARacingCar : public ACharacter
{
	DECLARE_CLASS(ARacingCar, ACharacter)

public:
	ARacingCar();
	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	
	void HandleScoreChanged(int32 NewScore);

	UTextRenderComponent* ScoreText = nullptr;
};
