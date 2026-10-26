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

	// 메뉴 그림 뒤에 깔리는 인게임 스크린샷
	UBillboardComponent* backgroundBillboard;

	int32 SelectedIndex = 0;
	bool bShowingHelp = false;
};
