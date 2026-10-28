#include "EnginePCH.h"
#include "SequenceSerializer.h"

namespace {
	struct FActionName { ESequenceAction Action; const char* Name; };

	const FActionName ActionName[] = {
		{ ESequenceAction::Wait, "Wait" },
		{ ESequenceAction::Finish, "Finish" },
	};

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

		Loaded.Push(Step);
	}

	OutSteps = Loaded;
	return true;
}