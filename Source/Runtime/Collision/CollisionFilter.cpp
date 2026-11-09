#include "EnginePCH.h"
#include "CollisionFilter.h"

ECollisionResponse FCollisionResponseContainer::GetResponse(ECollisionChannel Channel) const
{
	int32 Value;
	switch (Channel)
	{
	case ECollisionChannel::WorldStatic: Value = WorldStatic; break;
	case ECollisionChannel::WorldDynamic: Value = WorldDynamic; break;
	case ECollisionChannel::Pawn: Value = Pawn; break;
	case ECollisionChannel::Camera: Value = Camera; break;
	default: return ECollisionResponse::Ignore;
	}
	return static_cast<ECollisionResponse>(std::clamp(Value, 0, 2));
}

void FCollisionResponseContainer::SetResponse(ECollisionChannel Channel, ECollisionResponse Response)
{
	switch (Channel)
	{
	case ECollisionChannel::WorldStatic: WorldStatic = static_cast<int32>(Response); break;
	case ECollisionChannel::WorldDynamic: WorldDynamic = static_cast<int32>(Response); break;
	case ECollisionChannel::Pawn: Pawn = static_cast<int32>(Response); break;
	case ECollisionChannel::Camera: Camera = static_cast<int32>(Response); break;
	}
}

void FCollisionResponseContainer::SetAllResponses(ECollisionResponse Response)
{
	WorldStatic = WorldDynamic = Pawn = Camera = static_cast<int32>(Response);
}

FCollisionFilterData FCollisionFilterData::FromSettings(ECollisionChannel Channel, const FCollisionResponseContainer& Responses, const AActor* Owner)
{
	FCollisionFilterData Result;
	const int32 ChannelIndex = static_cast<int32>(Channel);
	if (ChannelIndex < 0 || ChannelIndex >= static_cast<int32>(ECollisionChannel::Count)) return Result;
	Result.ChannelBit = 1u << ChannelIndex;
	Result.Owner = Owner;
	for (int32 Index = 0; Index < static_cast<int32>(ECollisionChannel::Count); ++Index)
	{
		const ECollisionResponse Response = Responses.GetResponse(static_cast<ECollisionChannel>(Index));
		if (Response == ECollisionResponse::Block) Result.BlockMask |= 1u << Index;
		else if (Response == ECollisionResponse::Overlap) Result.OverlapMask |= 1u << Index;
	}
	return Result;
}

ECollisionResponse FCollisionFilterData::GetResponseTo(const FCollisionFilterData& Other) const
{
	const auto ResponseToChannel = [](const FCollisionFilterData& Filter, uint32 Channel)
	{
		if (Filter.BlockMask & Channel) return ECollisionResponse::Block;
		if (Filter.OverlapMask & Channel) return ECollisionResponse::Overlap;
		return ECollisionResponse::Ignore;
	};
	return std::min(ResponseToChannel(*this, Other.ChannelBit), ResponseToChannel(Other, ChannelBit));
}
