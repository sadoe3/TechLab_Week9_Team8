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
	virtual void Tick(float DeltaTime) override;

private:
	void ApplyMenuTexture();

	UCameraComponent* camera;
	UBillboardComponent* billboard;

	int32 SelectedIndex = 0;
	bool bShowingHelp = false;
};
