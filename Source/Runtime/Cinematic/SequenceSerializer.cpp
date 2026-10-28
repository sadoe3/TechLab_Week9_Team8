#include "EnginePCH.h"
#include "SequenceSerializer.h"
#include <cmath>

namespace {
	struct FActionName { ESequenceAction Action; const char* Name; };

	const FActionName ActionName[] = {
		{ ESequenceAction::Wait, "Wait" },
		{ ESequenceAction::Finish, "Finish" },
		{ ESequenceAction::SetViewTarget, "SetViewTarget" },
		{ ESequenceAction::Fade, "Fade" },
		{ ESequenceAction::LetterBox, "LetterBox" },
		{ ESequenceAction::MoveActor, "MoveActor" },
	};

	// 파일과 편집기가 공유하는 채널 이름
	const char* TrackTypeNames[] = {
		"Location.X", "Location.Y", "Location.Z",
		"Rotation.Pitch", "Rotation.Yaw", "Rotation.Roll",
		"Scale.X", "Scale.Y", "Scale.Z", "Camera.FOV",
		"Fade", "LetterBox", "ViewTarget"
	};
	const char* InterpolationNames[] = { "Constant", "Linear", "Cubic" };

}

int32 SequenceSerializer::GetTrackTypeCount()
{
	return static_cast<int32>(std::size(TrackTypeNames));
}

ESequenceTrackType SequenceSerializer::GetTrackTypeAt(int32 Index)
{
	assert(Index >= 0 && Index < GetTrackTypeCount());
	return static_cast<ESequenceTrackType>(Index);
}

const char* SequenceSerializer::ToString(ESequenceTrackType Type)
{
	const int32 Index = static_cast<int32>(Type);
	return Index >= 0 && Index < GetTrackTypeCount() ? TrackTypeNames[Index] : "Unknown";
}

bool SequenceSerializer::FromString(const FString& Name, ESequenceTrackType& OutType)
{
	for (int32 Index = 0; Index < GetTrackTypeCount(); ++Index)
	{
		if (Name == TrackTypeNames[Index])
		{
			OutType = static_cast<ESequenceTrackType>(Index);
			return true;
		}
	}
	return false;
}

const char* SequenceSerializer::ToString(ESequenceInterpolation Interpolation)
{
	const int32 Index = static_cast<int32>(Interpolation);
	return Index >= 0 && Index < static_cast<int32>(std::size(InterpolationNames))
		? InterpolationNames[Index] : "Unknown";
}

bool SequenceSerializer::FromString(const FString& Name, ESequenceInterpolation& OutInterpolation)
{
	for (int32 Index = 0; Index < static_cast<int32>(std::size(InterpolationNames)); ++Index)
	{
		if (Name == InterpolationNames[Index])
		{
			OutInterpolation = static_cast<ESequenceInterpolation>(Index);
			return true;
		}
	}
	return false;
}

int32 SequenceSerializer::GetActionCount()
{
	return static_cast<int32>(sizeof(ActionName) / sizeof(ActionName[0]));
}

ESequenceAction SequenceSerializer::GetActionAt(int32 Index)
{
	// 동일 이름 표를 통한 액션 열거
	assert(Index >= 0 && Index < GetActionCount());
	return ActionName[Index].Action;
}

const char* SequenceSerializer::ToString(ESequenceAction Action)
{
	for (const auto& Entry: ActionName) {
		if (Entry.Action == Action) return Entry.Name;
	}

	HTR_LOG(Error, "[Sequence] Unknown action: {}", (int32)Action);
	return "Unknown";
}

bool SequenceSerializer::FromString(const FString& Name, ESequenceAction& OutAction)
{
	for (const auto& Entry : ActionName) {
		if (Entry.Name == Name) {
			OutAction = Entry.Action;
			return true;
		}
	}

	return false;
}

bool SequenceSerializer::Save(const TArray<FSequenceStep>& Steps, const FString& Path)
{
	const std::filesystem::path FilePath(Path);
	if (FilePath.has_parent_path())
	{
		std::error_code ErrorCode;
		std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);
		if (ErrorCode)
		{
			HTR_LOG(Error, "[Sequence] Failed to create directory: {}", Path);
			return false;
		}
	}

	json Root;
	Root["Version"] = 1;
	Root["Steps"] = json::array();

	for (const FSequenceStep& Step : Steps)
	{
		json Entry;
		Entry["Duration"] = Step.Duration;
		Entry["Action"] = ToString(Step.Action);
		Entry["TargetName"] = Step.TargetName;
		
		json Params = json::object();
		for (const auto& Pair : Step.Params)
		{
			Params[Pair.first] = Pair.second;
		}
		Entry["Params"] = Params;

		Root["Steps"].push_back(Entry);
	}

	std::ofstream File(FilePath);
	if (!File.is_open())
	{
		HTR_LOG(Error, "[Sequence] Failed to open file: {}", Path);
		return false;
	}

	File << Root.dump(4);
	return true;
}

bool SequenceSerializer::Load(TArray<FSequenceStep>& OutSteps, const FString& Path)
{
	std::ifstream File(Path);
	if (!File.is_open())
	{
		HTR_LOG(Error, "[Sequence] Failed to open file: {}", Path);
		return false;
	}

	const json Root = json::parse(File, nullptr, false);
	if (Root.is_discarded())
	{
		HTR_LOG(Error, "[Sequence] Invalid JSON: {}", Path);
		return false;
	}

	const auto StepsNode = Root.find("Steps");
	if (StepsNode == Root.end() || !StepsNode->is_array())
	{
		HTR_LOG(Error, "[Sequence] Missing Steps array: {}", Path);
		return false;
	}

	TArray<FSequenceStep> Loaded;
	for (const json& Entry : *StepsNode)
	{
		if (!Entry.is_object())
		{
			HTR_LOG(Error, "[Sequence] Step is not an object: {}", Path);
			return false;
		}

		FSequenceStep Step;
		Step.Duration = Entry.value("Duration", 0.0f);
		Step.TargetName = Entry.value("TargetName", FString());

		const FString ActionName = Entry.value("Action", FString());
		if (!FromString(ActionName, Step.Action))
		{
			HTR_LOG(Error, "[Sequence] Unknown action '{}' in {}", ActionName, Path);
			return false;
		}

		const auto ParamsNode = Entry.find("Params");
		if (ParamsNode != Entry.end() && ParamsNode->is_object())
		{
			for (auto It = ParamsNode->begin(); It != ParamsNode->end(); ++It)
			{
				if (It->is_number())
				{
					Step.Params.Add(It.key(), It->get<float>());
				}
			}
		}

		Loaded.Push(Step);
	}

	OutSteps = Loaded;
	return true;
}

// 기존 순차 명령의 동일 대상·채널 병합
static FSequenceTrack& FindLegacyTrack(FSequenceData& Sequence, ESequenceTrackType Type, const FString& TargetName)
{
	for (FSequenceTrack& Track : Sequence.Tracks)
	{
		if (Track.Type == Type && Track.TargetName == TargetName)
		{
			return Track;
		}
	}
	FSequenceTrack Track;
	Track.Type = Type;
	Track.TargetName = TargetName;
	const int32 Index = static_cast<int32>(Sequence.Tracks.Add(Track));
	return Sequence.Tracks[Index];
}

// 생략된 시작값의 재생 시점 해석과 스텝 종료 후 값 유지
static void AddLegacySegment(FSequenceTrack& Track, float Start, float Duration, float To,
	ESequenceInterpolation Interpolation, const float* ExplicitFrom = nullptr)
{
	FSequenceKey First;
	First.Time = Start;
	First.Interpolation = Interpolation;
	if (ExplicitFrom)
	{
		First.Value = *ExplicitFrom;
	}
	else if (!Track.Keys.IsEmpty())
	{
		First.Value = Track.Keys.Last().Value;
		First.bUseInitialValue = Track.Keys.Last().bUseInitialValue;
	}
	else
	{
		First.bUseInitialValue = true;
	}
	Track.AddKey(First);
	FSequenceKey Last;
	Last.Time = Start + Duration;
	Last.Value = To;
	Last.Interpolation = ESequenceInterpolation::Constant;
	Track.AddKey(Last);
}

FSequenceData SequenceSerializer::ConvertLegacy(const TArray<FSequenceStep>& Steps)
{
	// 기존 스텝 순서의 누적 시간을 공통 시간축으로 이전
	FSequenceData Result;
	Result.Length = 0.0f;
	for (const FSequenceStep& Step : Steps)
	{
		const float Start = Result.Length;
		if (Step.Action == ESequenceAction::MoveActor)
		{
			const char* AxisNames[] = { "ToX", "ToY", "ToZ" };
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				if (const float* To = Step.Params.Find(AxisNames[Axis]))
				{
					FSequenceTrack& Track = FindLegacyTrack(Result, static_cast<ESequenceTrackType>(Axis), Step.TargetName);
					AddLegacySegment(Track, Start, Step.Duration, *To, ESequenceInterpolation::Cubic);
				}
			}
		}
		else if (Step.Action == ESequenceAction::Fade)
		{
			const float* FromParam = Step.Params.Find("From");
			const float* ToParam = Step.Params.Find("To");
			const float From = FromParam ? *FromParam : 0.0f;
			const float To = ToParam ? *ToParam : 1.0f;
			AddLegacySegment(FindLegacyTrack(Result, ESequenceTrackType::Fade, FString()),
				Start, Step.Duration, To, ESequenceInterpolation::Linear, &From);
		}
		else if (Step.Action == ESequenceAction::LetterBox)
		{
			const float* Ratio = Step.Params.Find("Ratio");
			AddLegacySegment(FindLegacyTrack(Result, ESequenceTrackType::LetterBox, FString()),
				Start, Step.Duration, Ratio ? *Ratio : 0.12f, ESequenceInterpolation::Linear);
		}
		else if (Step.Action == ESequenceAction::SetViewTarget)
		{
			FSequenceKey Key;
			Key.Time = Start;
			Key.TargetName = Step.TargetName;
			Key.Interpolation = ESequenceInterpolation::Constant;
			FindLegacyTrack(Result, ESequenceTrackType::ViewTarget, FString()).AddKey(Key);
		}
		Result.Length += Step.Duration;
	}
	return Result;
}

bool SequenceSerializer::Save(const FSequenceData& Sequence, const FString& Path)
{
	// 저장 전 공용 데이터 검증
	FString Error;
	if (!Sequence.Validate(Error))
	{
		HTR_LOG(Error, "[Sequence] {}", Error);
		return false;
	}
	json Root = {
		{ "Version", 2 }, { "Length", Sequence.Length }, { "Rate", Sequence.Rate },
		{ "Loop", Sequence.bLoop }, { "HoldEnd", Sequence.bHoldEnd }, { "Tracks", json::array() }
	};
	for (const FSequenceTrack& Track : Sequence.Tracks)
	{
		json Entry = { { "Type", ToString(Track.Type) }, { "TargetName", Track.TargetName }, { "Keys", json::array() } };
		for (const FSequenceKey& Key : Track.Keys)
		{
			json KeyEntry = { { "Time", Key.Time }, { "Value", Key.Value }, { "Interpolation", ToString(Key.Interpolation) } };
			if (!Key.TargetName.empty())
			{
				KeyEntry["TargetName"] = Key.TargetName;
			}
			if (Key.bUseInitialValue)
			{
				KeyEntry["UseInitial"] = true;
			}
			Entry["Keys"].push_back(std::move(KeyEntry));
		}
		Root["Tracks"].push_back(std::move(Entry));
	}

	const std::filesystem::path FilePath(Path);
	if (FilePath.has_parent_path())
	{
		std::error_code ErrorCode;
		std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);
		if (ErrorCode)
		{
			HTR_LOG(Error, "[Sequence] Failed to create directory: {}", Path);
			return false;
		}
	}
	std::ofstream File(FilePath);
	if (!File.is_open())
	{
		HTR_LOG(Error, "[Sequence] Failed to open file: {}", Path);
		return false;
	}
	File << Root.dump(4);
	return File.good();
}

bool SequenceSerializer::Load(FSequenceData& OutSequence, const FString& Path)
{
	std::ifstream File(Path);
	if (!File.is_open())
	{
		HTR_LOG(Error, "[Sequence] Failed to open file: {}", Path);
		return false;
	}
	// 파싱·검증 완료 전 기존 편집 데이터 유지
	try
	{
		const json Root = json::parse(File, nullptr, false);
		if (!Root.is_object())
		{
			HTR_LOG(Error, "[Sequence] Invalid JSON object: {}", Path);
			return false;
		}
		if (Root.contains("Version") && !Root["Version"].is_number_integer())
		{
			HTR_LOG(Error, "[Sequence] Version must be an integer: {}", Path);
			return false;
		}
		const int32 Version = Root.value("Version", 1);
		FSequenceData Loaded;
		if (Version == 1)
		{
			// 구형 로더가 무시하던 잘못된 파라미터 형식 차단
			const json& Entries = Root.at("Steps");
			if (!Entries.is_array())
			{
				return false;
			}
			for (const json& Entry : Entries)
			{
				if (Entry.contains("Params"))
				{
					const json& Params = Entry["Params"];
					if (!Params.is_object())
					{
						HTR_LOG(Error, "[Sequence] Params must be an object: {}", Path);
						return false;
					}
					for (const json& Value : Params)
					{
						if (!Value.is_number())
						{
							HTR_LOG(Error, "[Sequence] Parameter must be numeric: {}", Path);
							return false;
						}
					}
				}
			}
			TArray<FSequenceStep> Steps;
			if (!Load(Steps, Path))
			{
				return false;
			}
			for (const FSequenceStep& Step : Steps)
			{
				if (!std::isfinite(Step.Duration) || Step.Duration < 0.0f)
				{
					HTR_LOG(Error, "[Sequence] Invalid legacy duration: {}", Path);
					return false;
				}
				for (const auto& Pair : Step.Params)
				{
					if (!std::isfinite(Pair.second))
					{
						HTR_LOG(Error, "[Sequence] Invalid legacy parameter: {}", Path);
						return false;
					}
				}
			}
			Loaded = ConvertLegacy(Steps);
		}
		else if (Version == 2)
		{
			Loaded.Length = Root.at("Length").get<float>();
			Loaded.Rate = Root.value("Rate", 1.0f);
			Loaded.bLoop = Root.value("Loop", false);
			Loaded.bHoldEnd = Root.value("HoldEnd", true);
			const json& Tracks = Root.at("Tracks");
			if (!Tracks.is_array())
			{
				HTR_LOG(Error, "[Sequence] Tracks must be an array: {}", Path);
				return false;
			}
			for (const json& Entry : Tracks)
			{
				FSequenceTrack Track;
				if (!FromString(Entry.at("Type").get<FString>(), Track.Type))
				{
					HTR_LOG(Error, "[Sequence] Unknown track type: {}", Path);
					return false;
				}
				Track.TargetName = Entry.value("TargetName", FString());
				const json& Keys = Entry.at("Keys");
				if (!Keys.is_array())
				{
					HTR_LOG(Error, "[Sequence] Keys must be an array: {}", Path);
					return false;
				}
				for (const json& KeyEntry : Keys)
				{
					FSequenceKey Key;
					Key.Time = KeyEntry.at("Time").get<float>();
					Key.Value = KeyEntry.value("Value", 0.0f);
					Key.TargetName = KeyEntry.value("TargetName", FString());
					Key.bUseInitialValue = KeyEntry.value("UseInitial", false);
					if (!FromString(KeyEntry.value("Interpolation", FString("Cubic")), Key.Interpolation))
					{
						HTR_LOG(Error, "[Sequence] Unknown interpolation: {}", Path);
						return false;
					}
					Track.Keys.Add(Key);
				}
				Loaded.Tracks.Add(std::move(Track));
			}
		}
		else
		{
			HTR_LOG(Error, "[Sequence] Unsupported version: {}", Version);
			return false;
		}
		FString Error;
		if (!Loaded.Validate(Error))
		{
			HTR_LOG(Error, "[Sequence] {}: {}", Error, Path);
			return false;
		}
		OutSequence = std::move(Loaded);
		return true;
	}
	catch (const json::exception& Error)
	{
		HTR_LOG(Error, "[Sequence] Invalid file fields: {} ({})", Path, Error.what());
		return false;
	}
}
