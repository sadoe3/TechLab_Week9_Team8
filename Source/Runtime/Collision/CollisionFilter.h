#pragma once

#include "Core/Types.h"

class AActor;

enum class ECollisionEnabled : int32 { NoCollision, QueryOnly };
enum class ECollisionChannel : int32 { WorldStatic, WorldDynamic, Pawn, Camera, Count };
enum class ECollisionResponse : int32 { Ignore, Overlap, Block };

struct FCollisionResponseContainer
{
	// 기존 Int 프로퍼티로 저장하며 외부에서는 enum 접근 함수를 사용한다.
	int32 WorldStatic = static_cast<int32>(ECollisionResponse::Block);
	int32 WorldDynamic = static_cast<int32>(ECollisionResponse::Block);
	int32 Pawn = static_cast<int32>(ECollisionResponse::Block);
	int32 Camera = static_cast<int32>(ECollisionResponse::Block);

	ECollisionResponse GetResponse(ECollisionChannel Channel) const;
	void SetResponse(ECollisionChannel Channel, ECollisionResponse Response);
	void SetAllResponses(ECollisionResponse Response);
};

struct FCollisionFilterData
{
	uint32 ChannelBit = 0;
	uint32 BlockMask = 0;
	uint32 OverlapMask = 0;
	const AActor* Owner = nullptr;

	static FCollisionFilterData FromSettings(ECollisionChannel Channel, const FCollisionResponseContainer& Responses, const AActor* Owner);
	ECollisionResponse GetResponseTo(const FCollisionFilterData& Other) const;
};

struct FCollisionQueryParams
{
	const AActor* IgnoredOwner = nullptr;
	ECollisionChannel Channel = ECollisionChannel::WorldDynamic;
	FCollisionResponseContainer Responses;
	bool bIgnoreBlocks = false;
	// 이동용 Sweep: 시작 접촉에서 바깥쪽으로 이동할 때는 뒤의 Block까지 검사한다.
	bool bIgnoreInitialBlockingHitsWhenMovingOut = false;
};
