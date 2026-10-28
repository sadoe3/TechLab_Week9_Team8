#pragma once

#include "EnginePCH.h"
#include "Engine/World.h"

enum class ESequenceAction {
	Wait, Finish, SetViewTarget, Fade, LetterBox, MoveActor
};

struct FSequenceStep {
	float Duration = 0.0f;
	ESequenceAction Action = ESequenceAction::Wait;
	FString TargetName;
	TMap <FString, float> Params;
};

// 공통 시간축에서 평가할 속성 채널
enum class ESequenceTrackType
{
	LocationX, LocationY, LocationZ,
	RotationPitch, RotationYaw, RotationRoll,
	ScaleX, ScaleY, ScaleZ, CameraFOV,
	Fade, LetterBox, ViewTarget
};

enum class ESequenceInterpolation { Constant, Linear, Cubic };

// 숫자 채널 또는 시점 전환 키
struct FSequenceKey
{
	float Time = 0.0f;
	float Value = 0.0f;
	ESequenceInterpolation Interpolation = ESequenceInterpolation::Cubic;
	FString TargetName;
	bool bUseInitialValue = false; // 기존 파일의 생략된 시작값
	FViewTargetTransitionParams BlendParams; // ViewTarget 키의 전환 설정
};

struct FSequenceTrack
{
	ESequenceTrackType Type = ESequenceTrackType::LocationX;
	FString TargetName;
	TArray<FSequenceKey> Keys;

	// 시간순 삽입과 동일 시각 교체
	int32 AddKey(const FSequenceKey& Key);
	float Evaluate(float Time, float InitialValue) const;
};

// 저장·편집·재생에 공용으로 사용하는 시퀀스 데이터
struct FSequenceData
{
	float Length = 5.0f;
	float Rate = 1.0f;
	bool bLoop = false;
	bool bHoldEnd = true;
	TArray<FSequenceTrack> Tracks;
	bool Validate(FString& OutError) const;
};


class FSequencePlayer {
public:
	static void Play(const TArray <FSequenceStep> & InSteps, UWorld * World);
	static void Play(const FSequenceData& InSequence, UWorld* World);
	static void Tick();
	static void Stop();
	static bool IsPlaying();
	static void SetPaused(bool bInPaused);
	static bool IsPaused() { return bPaused; }
	static void Seek(float Time);
	static float GetPosition() { return Position; }
	static uint32 GetPlaybackGeneration() { return PlaybackGeneration; }
	static float GetTrackValue(const FSequenceTrack& Track, UWorld* World);

private:
	friend class UWorld;

	// 소유 World의 시간·종료 경계 연결
	static void TickWorld(UWorld* World, float DeltaTime);
	static void StopWorld(UWorld* World);
	static void Evaluate(bool bRestore = false);
	static AActor* FindActor(UWorld* World, const FString& Name);
	static UCameraComponent* FindCamera(AActor* Actor);

	// 재생 시작 시 고정한 대상과 원래 속성값
	inline static FSequenceData Sequence;
	inline static TArray<TWeakObjectPtr<AActor>> BoundActors;
	inline static TArray<float> InitialValues;
	inline static TWeakObjectPtr<AActor> InitialViewTarget;
	inline static float Position = 0.0f;
	inline static bool bPlaying = false;
	inline static bool bPaused = false;
	inline static int32 LastCameraKey = -1;
	// 콜백에서 발생한 재생 교체 식별
	inline static uint32 PlaybackGeneration = 0;

	inline static TWeakObjectPtr<UWorld> WorldPtr;

};
