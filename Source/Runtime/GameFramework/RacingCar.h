#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Core/Misc/Delegate.h"

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
    void UpdateReverseCameraLag();
    bool bCameraLagSuppressedForReverse = false;
    bool bCameraLagEnabledBeforeReverse = false;

	// 카메라에 붙일 리더보드 텍스트를 생성합니다.
	void CreateLeaderboardHud();
	// 결과 화면 상태에 따라 리더보드를 표시하거나 숨깁니다.
	void UpdateLeaderboardHud();
	// 확정된 기록을 화면에 표시할 문자열로 바꿉니다.
	void UpdateLeaderboardText();
	// 항목은 왼쪽, 기록은 오른쪽 기준선에 맞춰 배치합니다.
	void UpdateLeaderboardLayout();
	// 다시 도전 / 메인 메뉴 / 크레딧 선택 입력을 처리합니다.
	void HandleLeaderboardInput();
	// 리더보드와 크레딧 화면을 맞바꿔 표시합니다.
	void SetCreditsVisible(bool bVisible);

	UTextRenderComponent* LeaderboardTitleText = nullptr;
	UTextRenderComponent* BestScoreText = nullptr;
	UTextRenderComponent* BestFinishTimeText = nullptr;
	UTextRenderComponent* FinishScoreText = nullptr;
	UTextRenderComponent* FinishTimeText = nullptr;
	UTextRenderComponent* RetryText = nullptr;
	UTextRenderComponent* MainMenuText = nullptr;
	UTextRenderComponent* BestRecordHeaderText = nullptr;
	UTextRenderComponent* CurrentRecordHeaderText = nullptr;
	UTextRenderComponent* BestScoreLabelText = nullptr;
	UTextRenderComponent* BestFinishTimeLabelText = nullptr;
	UTextRenderComponent* FinishScoreLabelText = nullptr;
	UTextRenderComponent* FinishTimeLabelText = nullptr;
	UTextRenderComponent* ResultMenuPointerText = nullptr;
	UTextRenderComponent* CreditsMenuText = nullptr;
	// 크레딧 화면은 리더보드와 번갈아 표시하므로 따로 모읍니다.
	UTextRenderComponent* CreditsTitleText = nullptr;
	UTextRenderComponent* CreditsTeamText = nullptr;
	UTextRenderComponent* CreditsMemberAText = nullptr;
	UTextRenderComponent* CreditsMemberBText = nullptr;
	UTextRenderComponent* CreditsMemberCText = nullptr;
	UTextRenderComponent* CreditsMemberDText = nullptr;
	UTextRenderComponent* CreditsBackText = nullptr;
	TArray<UTextRenderComponent*> CreditsTexts;
	bool bCreditsVisible = false;
	TArray<UTextRenderComponent*> LeaderboardTexts;

	// 0은 다시 도전, 1은 메인 메뉴입니다.
	int32 SelectedResultMenu = 0;
	bool bLeaderboardVisible = false;
	// 완주 UI에서 사용한 Enter를 놓은 뒤 새 입력을 받습니다.
	bool bWaitingForConfirmRelease = true;

	void HandleComponentHit(UPrimitiveComponent* HitComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit);
	void HandleSlomoFinished();
};
