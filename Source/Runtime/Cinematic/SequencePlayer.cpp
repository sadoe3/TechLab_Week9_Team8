#include "EnginePCH.h"
#include "SequencePlayer.h"
#include "SequenceSerializer.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/TimeDilation.h"
#include "Engine/Level.h"
#include <algorithm>
#include <cmath>

int32 FSequenceTrack::AddKey(const FSequenceKey& Key)
{
	// 같은 시각의 값 교체와 정렬 유지
	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		if (Keys[Index].Time == Key.Time)
		{
			Keys[Index] = Key;
			return Index;
		}
		if (Keys[Index].Time > Key.Time)
		{
			Keys.Insert(Key, Index);
			return Index;
		}
	}
	return static_cast<int32>(Keys.Add(Key));
}

float FSequenceTrack::Evaluate(float Time, float InitialValue) const
{
	// 첫 키 이전에는 재생 시작 시 값 유지
	if (Keys.IsEmpty() || Time < Keys[0].Time)
	{
		return InitialValue;
	}
	for (int32 Index = 1; Index < Keys.Num(); ++Index)
	{
		const FSequenceKey& Right = Keys[Index];
		if (Time < Right.Time)
		{
			const FSequenceKey& Left = Keys[Index - 1];
			const float From = Left.bUseInitialValue ? InitialValue : Left.Value;
			const float To = Right.bUseInitialValue ? InitialValue : Right.Value;
			if (Left.Interpolation == ESequenceInterpolation::Constant)
			{
				return From;
			}
			double Alpha = (Time - Left.Time) / (Right.Time - Left.Time);
			if (Left.Interpolation == ESequenceInterpolation::Cubic)
			{
				Alpha = Alpha * Alpha * (3.0 - 2.0 * Alpha);
			}
			return static_cast<float>(From * (1.0 - Alpha) + To * Alpha);
		}
	}
	const FSequenceKey& Last = Keys.Last();
	return Last.bUseInitialValue ? InitialValue : Last.Value;
}

bool FSequenceData::Validate(FString& OutError) const
{
	// 저장·재생 경계의 공통 검증
	OutError.clear();
	if (!std::isfinite(Length) || Length < 0.0f || !std::isfinite(Rate) || Rate <= 0.0f)
	{
		OutError = "Length must be non-negative and Rate must be positive.";
		return false;
	}
	for (int32 Index = 0; Index < Tracks.Num(); ++Index)
	{
		const FSequenceTrack& Track = Tracks[Index];
		const int32 Type = static_cast<int32>(Track.Type);
		const bool bGlobal = Track.Type >= ESequenceTrackType::Fade;
		if (Type < 0 || Type > static_cast<int32>(ESequenceTrackType::ViewTarget)
			|| (!bGlobal && Track.TargetName.empty()))
		{
			OutError = "Actor tracks require a target name and a valid channel.";
			return false;
		}
		for (int32 Previous = 0; Previous < Index; ++Previous)
		{
			if (Tracks[Previous].Type == Track.Type && (bGlobal || Tracks[Previous].TargetName == Track.TargetName))
			{
				OutError = "Only one track per actor property or global channel is allowed.";
				return false;
			}
		}
		float PreviousTime = -1.0f;
		float PreviousBlendEnd = 0.0f;
		for (const FSequenceKey& Key : Track.Keys)
		{
			const int32 Interpolation = static_cast<int32>(Key.Interpolation);
			if (!std::isfinite(Key.Time) || !std::isfinite(Key.Value) || Key.Time < 0.0f
				|| Key.Time > Length || Key.Time <= PreviousTime || Interpolation < 0 || Interpolation > 2
				|| (Track.Type == ESequenceTrackType::ViewTarget && Key.TargetName.empty()))
			{
				OutError = "Keys require unique sorted times within Length, finite values and valid interpolation/targets.";
				return false;
			}
			if (Track.Type == ESequenceTrackType::ViewTarget)
			{
				// 전환 종료 시각과 같은 트랙 내 구간 중첩 검증
				const FViewTargetTransitionParams& Params = Key.BlendParams;
				const int32 Function = static_cast<int32>(Params.BlendFunction);
				if (!std::isfinite(Params.BlendTime) || Params.BlendTime < 0.0f
					|| !std::isfinite(Params.BlendExp) || Params.BlendExp <= 0.0f
					|| Function < 0 || Function > static_cast<int32>(EViewTargetBlendFunction::EaseInOut)
					|| Key.Time + Params.BlendTime > Length || Key.Time < PreviousBlendEnd)
				{
					OutError = "Camera blends require valid settings, must end within Length and cannot overlap the next key.";
					return false;
				}
				PreviousBlendEnd = Key.Time + Params.BlendTime;
			}
			PreviousTime = Key.Time;
		}
	}
	return true;
}

AActor* FSequencePlayer::FindActor(UWorld* World, const FString& Name)
{
	// 표시 라벨과 분리된 실제 객체 이름 조회
	ULevel* Level = World ? World->GetPersistentLevel() : nullptr;
	if (Level)
	{
		const FName SearchName(Name);
		for (AActor* Actor : Level->GetActors())
		{
			if (Actor && !Actor->IsPendingDestroy() && Actor->GetFName() == SearchName)
			{
				return Actor;
			}
		}
	}
	return nullptr;
}

UCameraComponent* FSequencePlayer::FindCamera(AActor* Actor)
{
	// CalcCamera와 동일한 활성 카메라 선택
	if (Actor)
	{
		for (UActorComponent* Component : Actor->GetComponents())
		{
			UCameraComponent* Camera = Cast<UCameraComponent>(Component);
			if (Camera && Camera->IsActive() && !Camera->IsPendingDestroy())
			{
				return Camera;
			}
		}
	}
	return nullptr;
}

float FSequencePlayer::GetTrackValue(const FSequenceTrack& Track, UWorld* World)
{
	// 키 기록과 재생 초기값에 공용으로 사용하는 속성 조회
	APlayerCameraManager* Manager = World ? World->GetPlayerCameraManager() : nullptr;
	if (Track.Type == ESequenceTrackType::Fade)
	{
		return Manager ? Manager->GetFadeAmount() : 0.0f;
	}
	if (Track.Type == ESequenceTrackType::LetterBox)
	{
		return Manager ? Manager->GetLetterBoxHeightRatio() : 0.0f;
	}
	AActor* Actor = FindActor(World, Track.TargetName);
	USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
	const int32 Type = static_cast<int32>(Track.Type);
	if (Type >= 0 && Type <= 2)
	{
		return Root ? Root->GetWorldLocation()[Type] : 0.0f;
	}
	if (Type >= 3 && Type <= 5)
	{
		return Root ? Root->GetWorldRotation()[Type - 3] : 0.0f;
	}
	if (Type >= 6 && Type <= 8)
	{
		return Root ? Root->GetWorldScale3D()[Type - 6] : 1.0f;
	}
	UCameraComponent* Camera = FindCamera(Actor);
	return Track.Type == ESequenceTrackType::CameraFOV && Camera ? Camera->GetFieldOfView() : 0.0f;
}

void FSequencePlayer::Play(const TArray<FSequenceStep>& InSteps, UWorld* World)
{
	// 기존 호출부의 순차 스텝을 시간축 데이터로 이전
	Play(SequenceSerializer::ConvertLegacy(InSteps), World);
}

void FSequencePlayer::Play(const FSequenceData& InSequence, UWorld* World)
{
	FString Error;
	if (!World || !World->AcceptsChanges() || !InSequence.Validate(Error))
	{
		HTR_LOG(Error, "[Sequence] Cannot play: {}", Error);
		return;
	}
	Stop();
	Sequence = InSequence;
	WorldPtr = World;
	InitialViewTarget = World->GetViewTarget();
	LastCameraKey = -2;
	for (const FSequenceTrack& Track : Sequence.Tracks)
	{
		AActor* Actor = Track.Type < ESequenceTrackType::Fade ? FindActor(World, Track.TargetName) : nullptr;
		BoundActors.Add(TWeakObjectPtr<AActor>(Actor));
		InitialValues.Add(GetTrackValue(Track, World));
		if (!Actor && Track.Type < ESequenceTrackType::Fade)
		{
			HTR_LOG(Warning, "[Sequence] Target not found: {}", Track.TargetName);
		}
	}
	bPlaying = true;
	Evaluate();
}

void FSequencePlayer::Tick()
{
	// 기존 외부 호출의 게임 시간 기준 유지
	TickWorld(WorldPtr.Get(), FTimeDilation::GetGameDelta());
}

void FSequencePlayer::TickWorld(UWorld* World, float DeltaTime)
{
	if (!bPlaying || bPaused || WorldPtr.Get() != World)
	{
		return;
	}
	if (!WorldPtr)
	{
		Stop();
		return;
	}
	if (!std::isfinite(DeltaTime) || DeltaTime < 0.0f)
	{
		return;
	}
	// 모든 트랙에 동일한 절대 시각 적용
	const double NextPosition = Position + static_cast<double>(DeltaTime) * Sequence.Rate;
	const bool bReachedEnd = NextPosition >= Sequence.Length;
	if (bReachedEnd && Sequence.bLoop && Sequence.Length > 0.0f)
	{
		Position = static_cast<float>(std::fmod(NextPosition, Sequence.Length));
		LastCameraKey = -2;
	}
	else
	{
		Position = static_cast<float>(std::min(NextPosition, static_cast<double>(Sequence.Length)));
	}
	const uint32 UpdatingGeneration = PlaybackGeneration;
	Evaluate();
	if (PlaybackGeneration != UpdatingGeneration)
	{
		return;
	}
	if (bReachedEnd && (!Sequence.bLoop || Sequence.Length == 0.0f))
	{
		if (!Sequence.bHoldEnd)
		{
			Evaluate(true);
		}
		if (PlaybackGeneration == UpdatingGeneration)
		{
			bPlaying = false;
			bPaused = false;
		}
	}
}

void FSequencePlayer::Evaluate(bool bRestore)
{
	UWorld* World = WorldPtr.Get();
	if (!World)
	{
		return;
	}
	const uint32 UpdatingGeneration = PlaybackGeneration;
	for (int32 Index = 0; Index < Sequence.Tracks.Num(); ++Index)
	{
		const FSequenceTrack& Track = Sequence.Tracks[Index];
		if (Track.Keys.IsEmpty() || Track.Type >= ESequenceTrackType::Fade)
		{
			continue;
		}
		AActor* Actor = BoundActors[Index].Get();
		if (!Actor || Actor->IsPendingDestroy() || !Actor->GetRootComponent())
		{
			continue;
		}
		bool bAlreadyApplied = false;
		for (int32 Previous = 0; Previous < Index; ++Previous)
		{
			if (!Sequence.Tracks[Previous].Keys.IsEmpty() && BoundActors[Previous].Get() == Actor)
			{
				bAlreadyApplied = true;
				break;
			}
		}
		if (bAlreadyApplied)
		{
			continue;
		}
		// 같은 액터의 축을 합성한 뒤 Transform을 한 번 적용
		USceneComponent* Root = Actor->GetRootComponent();
		FVector Location = Root->GetWorldLocation();
		FRotator Rotation = Root->GetWorldRotation();
		FVector Scale = Root->GetWorldScale3D();
		bool bTransform = false;
		bool bScale = false;
		for (int32 Channel = Index; Channel < Sequence.Tracks.Num(); ++Channel)
		{
			const FSequenceTrack& Property = Sequence.Tracks[Channel];
			if (BoundActors[Channel].Get() != Actor || Property.Keys.IsEmpty())
			{
				continue;
			}
			const float Value = bRestore ? InitialValues[Channel] : Property.Evaluate(Position, InitialValues[Channel]);
			const int32 Type = static_cast<int32>(Property.Type);
			if (Type <= 2)
			{
				Location[Type] = Value;
				bTransform = true;
			}
			else if (Type <= 5)
			{
				Rotation[Type - 3] = Value;
				bTransform = true;
			}
			else if (Type <= 8)
			{
				Scale[Type - 6] = Value;
				bScale = true;
			}
			else if (Property.Type == ESequenceTrackType::CameraFOV)
			{
				if (UCameraComponent* Camera = FindCamera(Actor))
				{
					Camera->SetFieldOfView(std::clamp(Value, 1.0f, 179.0f));
				}
			}
		}
		if (bScale)
		{
			Root->SetWorldScale3D(Scale);
		}
		if (bTransform)
		{
			Root->SetWorldLocationAndRotation(Location, Rotation, false);
		}
		if (PlaybackGeneration != UpdatingGeneration)
		{
			return;
		}
	}
	// 화면 효과와 카메라 컷도 동일 시각으로 평가
	for (int32 Index = 0; Index < Sequence.Tracks.Num(); ++Index)
	{
		const FSequenceTrack& Track = Sequence.Tracks[Index];
		APlayerCameraManager* Manager = World->GetPlayerCameraManager();
		if (Track.Keys.IsEmpty() || !Manager)
		{
			continue;
		}
		const float Value = bRestore ? InitialValues[Index] : Track.Evaluate(Position, InitialValues[Index]);
		if (Track.Type == ESequenceTrackType::Fade)
		{
			const float Amount = std::clamp(Value, 0.0f, 1.0f);
			Manager->StartCameraFade(Amount, Amount, 0.0f, FLinearColor(0, 0, 0), true);
		}
		else if (Track.Type == ESequenceTrackType::LetterBox)
		{
			Manager->StartCameraLetterBox(std::clamp(Value, 0.0f, 0.5f), 0.0f);
		}
		else if (Track.Type == ESequenceTrackType::ViewTarget)
		{
			int32 KeyIndex = -1;
			for (int32 Key = 0; !bRestore && Key < Track.Keys.Num() && Track.Keys[Key].Time <= Position; ++Key)
			{
				KeyIndex = Key;
			}
			const bool bTimedKey = KeyIndex >= 0 && Track.Keys[KeyIndex].BlendParams.BlendTime > 0.0f;
			if (KeyIndex != LastCameraKey || bRestore || bTimedKey)
			{
				AActor* Target = KeyIndex < 0 ? InitialViewTarget.Get() : FindActor(World, Track.Keys[KeyIndex].TargetName);
				LastCameraKey = KeyIndex;
				if (Target && !Target->IsPendingDestroy())
				{
					if (bTimedKey)
					{
						// 직전 키 대상에서 현재 키 대상으로 절대 시간 보간
						AActor* From = KeyIndex > 0 ? FindActor(World, Track.Keys[KeyIndex - 1].TargetName) : InitialViewTarget.Get();
						const FSequenceKey& Key = Track.Keys[KeyIndex];
						World->SetViewTargetBlendAtTime(From, Target, Key.BlendParams, Position - Key.Time);
					}
					else
					{
						World->SetViewTarget(Target);
					}
				}
				else if (KeyIndex >= 0)
				{
					HTR_LOG(Warning, "[Sequence] Camera target not found: {}", Track.Keys[KeyIndex].TargetName);
				}
			}
		}
		if (PlaybackGeneration != UpdatingGeneration)
		{
			return;
		}
	}
}

void FSequencePlayer::SetPaused(bool bInPaused)
{
	// 일시정지 중에도 시퀀스 소유권 유지
	bPaused = bPlaying && bInPaused;
}

void FSequencePlayer::Seek(float Time)
{
	// 원래 속성 스냅샷을 기준으로 앞뒤 탐색
	if (!WorldPtr || !std::isfinite(Time))
	{
		return;
	}
	Position = std::clamp(Time, 0.0f, Sequence.Length);
	bPlaying = true;
	bPaused = true;
	Evaluate();
	if (UWorld* World = WorldPtr.Get())
	{
		// World 시간이 멈춘 상태에서도 탐색 결과를 즉시 표시
		if (APlayerCameraManager* Manager = World->GetPlayerCameraManager())
		{
			Manager->UpdateCamera(0.0f);
		}
	}
}

void FSequencePlayer::StopWorld(UWorld* World)
{
	// 다른 World에서 재생 중인 시퀀스 유지
	if (WorldPtr.Get() == World)
	{
		Stop();
	}
}

void FSequencePlayer::Stop()
{
	if (UWorld* World = WorldPtr.Get())
	{
		// 시퀀스 종료 후 외부 시간에 고정된 전환 해제
		if (APlayerCameraManager* Manager = World->GetPlayerCameraManager())
		{
			if (Manager->IsViewTargetBlendExternallyTimed())
			{
				Manager->CancelViewTargetBlend();
			}
		}
	}
	// 기존 Stop과 동일하게 위치 결과를 남기고 참조 해제
	++PlaybackGeneration;
	bPlaying = false;
	bPaused = false;
	Position = 0.0f;
	Sequence = FSequenceData();
	BoundActors.Reset();
	InitialValues.Reset();
	InitialViewTarget.Reset();
	WorldPtr.Reset();
	LastCameraKey = -2;
}

bool FSequencePlayer::IsPlaying()
{
	return bPlaying;
}
