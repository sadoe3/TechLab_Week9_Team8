#pragma once

#include <cstddef>
#include "Core/Types.h"
#include "UObject/Casts.h"
#include "UObject/UObjectHash.h"

// UObject의 UUID 기반 비소유 참조
template<typename T>
class TWeakObjectPtr
{
public:
	// 대상 지정·빈 참조 초기화
	TWeakObjectPtr(T* Object = nullptr)
	{
		*this = Object;
	}

	// 대상 식별자 저장
	TWeakObjectPtr& operator=(T* Object)
	{
		ObjectUUID = Object ? Object->GetUUID() : 0;
		return *this;
	}

	// 생존 객체 조회
	T* Get() const
	{
		return Cast<T>(FindObjectByUUID(ObjectUUID));
	}

	// 참조 해제
	void Reset()
	{
		ObjectUUID = 0;
	}

	bool IsStale() const { return ObjectUUID != 0 && Get() == nullptr; }

	// 생존 여부 판정
	explicit operator bool() const
	{
		return Get() != nullptr;
	}

	// 빈 참조 비교
	bool operator==(std::nullptr_t) const
	{
		return Get() == nullptr;
	}

	// 유효 참조 비교
	bool operator!=(std::nullptr_t) const
	{
		return Get() != nullptr;
	}

	// 조회 대상 접근
	T* operator->() const
	{
		return Get();
	}

private:
	// 대상 식별자
	uint32 ObjectUUID = 0;
};
