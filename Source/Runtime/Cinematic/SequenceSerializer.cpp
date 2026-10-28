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