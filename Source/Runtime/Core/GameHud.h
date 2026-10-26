#pragma once

#include "Types.h"
#include "EngineString.h"
#include "Core/Misc/Delegate.h"

DECLARE_DELEGATE(ScoreUpdate, int32);
DECLARE_DELEGATE(CountdownUpdate, const FString&);

// 레이싱 씬 위에 표시할 결과 화면입니다.
enum class ERaceResultScreen
{
	Hidden,
	Finish,
	Leaderboard
};

// 결과 UI에서 사용할 이번 주행 결과와 역대 최고 기록입니다.
struct FRaceResult
{
	int32 FinishScore = 0;
	double FinishTime = 0.0;
	int32 BestScore = 0;
	double BestFinishTime = 0.0;
};

class FGameHud
{
public:
	static int32 GetScore() { return Score; }
	static void SetScore(int32 inScore) {
		Score = inScore;
		OnScoreChanged.Broadcast(Score);
	}
	static void AddScore(int32 inScore) { Score += inScore; }

	inline static ScoreUpdate OnScoreChanged;

	static const FString& GetCountdown() { return Countdown; }
	static void SetCountdown(const FString& inCountdown) {
		Countdown = inCountdown;
		OnCountdownChanged.Broadcast(Countdown);
	}

	inline static CountdownUpdate OnCountdownChanged;

	static float GetRaceTime() { return RaceTime; }
	static void SetRaceTime(float inRaceTime) { RaceTime = inRaceTime; }

	static ERaceResultScreen GetResultScreen() { return ResultScreen; }
	static void SetResultScreen(ERaceResultScreen InScreen)
	{
		ResultScreen = InScreen;
	}

	// 완주 UI를 숨기거나 종료 연출을 마친 뒤 담당 UI에서 호출합니다.
	static void NotifyFinishUIClosed()
	{
		// 완주 UI 단계에서만 전환하며, 중복 호출은 무시합니다.
		if (ResultScreen != ERaceResultScreen::Finish)
		{
			return;
		}

		// 기존 레이싱 씬을 유지하고 리더보드 표시 상태로 변경합니다.
		ResultScreen = ERaceResultScreen::Leaderboard;
	}

	// Lua에서 확정한 결과를 복사하고, UI에는 읽기 전용으로 제공합니다.
	static void SetRaceResult(const FRaceResult& InResult)
	{
		RaceResult = InResult;
	}
	static const FRaceResult& GetRaceResult() { return RaceResult; }

private:
	inline static int32 Score = 0;
	inline static FString Countdown;
	inline static float RaceTime = 0.0f;
	inline static ERaceResultScreen ResultScreen = ERaceResultScreen::Hidden;
	inline static FRaceResult RaceResult;
};
