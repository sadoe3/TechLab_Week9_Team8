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
