#pragma once

#include "SequencePlayer.h"


namespace SequenceSerializer {
	const char* ToString(ESequenceAction Action);
	bool FromString(const FString& Name, ESequenceAction& OutAction);
	// 편집기용 액션 목록 조회
	int32 GetActionCount();
	ESequenceAction GetActionAt(int32 Index);
	bool Save(const TArray<FSequenceStep>& Steps, const FString& Path);
	bool Load(TArray<FSequenceStep>& OutSteps, const FString& Path);

	// 트랙 형식 저장과 기존 스텝 파일 변환
	bool Save(const FSequenceData& Sequence, const FString& Path);
	bool Load(FSequenceData& OutSequence, const FString& Path);
	FSequenceData ConvertLegacy(const TArray<FSequenceStep>& Steps);
	const char* ToString(ESequenceTrackType Type);
	bool FromString(const FString& Name, ESequenceTrackType& OutType);
	const char* ToString(ESequenceInterpolation Interpolation);
	bool FromString(const FString& Name, ESequenceInterpolation& OutInterpolation);
	// 시점 전환 보간 이름 변환
	const char* ToString(EViewTargetBlendFunction BlendFunction);
	bool FromString(const FString& Name, EViewTargetBlendFunction& OutBlendFunction);
	int32 GetTrackTypeCount();
	ESequenceTrackType GetTrackTypeAt(int32 Index);
}
