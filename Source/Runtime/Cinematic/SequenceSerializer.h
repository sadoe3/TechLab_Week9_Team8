#pragma once

#include "SequencePlayer.h"


namespace SequenceSerializer {
	const char* ToString(ESequenceAction Action);
	bool FromString(const FString& Name, ESequenceAction& OutAction);
}	