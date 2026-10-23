#pragma once

#include "Containers/Array.h"
#include <functional>
#include "Core/Types.h"


struct FDelegateHandle {
	int64 ID;
	inline static int64 DelegateID = 0;

	FDelegateHandle() : ID(0) {};

	FDelegateHandle(int64 InValue) : ID(InValue) {};

	static FDelegateHandle Generate() {
		return ++DelegateID;
	}
	
	bool IsValid() {
		return ID != 0;
	}

	bool operator==(const FDelegateHandle& other) const {
		return this->ID == other.ID;
	}

	bool operator!=(const FDelegateHandle& other) const {
		return this->ID != other.ID;
	}
};

// UE의 Delegate 축소판
template <typename... Args>
class TMulticastDelegate
{
	using FuncType = std::function<void(Args...)>;

	struct FHandlerEntry {
		FuncType func;
		FDelegateHandle ID;
		void* Owner;		// Owner : 구독자
	};

public:
	FDelegateHandle Add(FuncType InFunc) {
		FHandlerEntry handle;
		handle.ID = FDelegateHandle::Generate();
		handle.func = InFunc;
		handle.Owner = nullptr;
		Handlers.Push(handle);
		return handle.ID;
	}

	void Remove(FDelegateHandle inHandler) {
		for (int32 i = Handlers.Num() - 1; i >= 0; i--) {
			if (Handlers[i].ID == inHandler) {
				Handlers.RemoveAt(i);
				return;
			}
		}
	}

	void Clear() {
		Handlers.Empty();
	}

	void RemoveAll(void* InOwner) {
		for (int32 i = Handlers.Num() - 1; i >= 0; i--) {
			if (Handlers[i].Owner == InOwner) {
				Handlers.RemoveAt(i);
			}
		}
	}

	int32 Num() const {
		return Handlers.Num();
	}

	void Broadcast(Args... args) {
		TArray <FHandlerEntry> HandlerList = Handlers;	//TArray는 vector기반 -> 삭제시 오류남! 
		for (auto & Entry : HandlerList) {
			Entry.func(args...);
		}
	}

	bool IsBound() const{
		return !Handlers.IsEmpty();
	}

private:
	TArray <FHandlerEntry> Handlers;

};
