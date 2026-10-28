#pragma once

#include "SequencePlayer.h"


namespace SequenceSerializer {
	const char* ToString(ESequenceAction Action);
	bool FromString(const FString& Name, ESequenceAction& OutAction);
	bool Save(const TArray<FSequenceStep>& Steps, const FString& Path);
	bool Load(TArray<FSequenceStep>& OutSteps, const FString& Path);
}	