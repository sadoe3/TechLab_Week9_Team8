#pragma once

#include "GameFramework/Actor.h"

class UCameraComponent;
class UBillboardComponent;

class AMainMenuHud : public AActor
{
	DECLARE_CLASS(AMainMenuHud, AActor)
public:
	AMainMenuHud();
	virtual void BeginPlay() override;

private:
	UCameraComponent* camera;
	UBillboardComponent* billboard;
};

