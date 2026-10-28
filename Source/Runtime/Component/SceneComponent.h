#pragma once

#include "../Math/Transform.h"
#include "ActorComponent.h"
#include "Math/Box.h"
#include "Collision/HitResult.h"

enum class EAttachmentRule
{
	KeepRelative,
	KeepWorld,
	SnapToTarget
};

class USceneComponent : public UActorComponent
{
	DECLARE_CLASS(USceneComponent, UActorComponent)

	REFLECT_START(ClassName)
		PROPERTY(Transform)
		REFLECT_END()

public:
	USceneComponent() = default;
	virtual ~USceneComponent() override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	// Get & Set
	const FVector& GetRelativeLocation() const { return Transform.Location; }
	void SetRelativeLocation(const FVector& InLocation);


	const FRotator& GetRelativeRotation() const { return Transform.Rotation; }
	void SetRelativeRotation(const FRotator& InRotation);
	void SetRelativeLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation);
	const FVector& GetRelativeScale3D() const { return Transform.Scale; }
	void SetRelativeScale3D(const FVector& InScale);

	// 쿼터니언 적용된 회전행렬
	FQuat GetRelativeRotationQuat() const { return Transform.GetOrientation(); }

	const FTransform& GetTransform() const { return Transform; }
	void SetTransform(const FTransform& InTransform);

	// Attatch-To
	USceneComponent* GetAttachParent() const { return AttachParent; }
	const TArray<USceneComponent*>& GetAttachChildren() const { return AttachChildren; }

	// todo: 현재는 socket의 기능이 구현되어 있지 않아서, 임시적으로 이 방식을 통해 socket의 개념을 적용하였다.
	// todo: 나중에 scoket이 구현되면, 이 함수는 제거해야한다 (현재 이 방식을 이용하면 socket을 최대 1개만 사용할 수 있기때문)
	// 이 함수는 자식의 상대 변환이 기준으로 삼는 월드 변환을 반환한다.
	// 기본은 이 Component의 월드 변환이다.
	FTransform GetTransformForAttachment() const;
	virtual FRotator GetRotationForAttachment() const;
	virtual FVector GetLocationForAttachment() const;

	void SetupAttachment(USceneComponent* InParent, EAttachmentRule Rule = EAttachmentRule::KeepRelative);
	void DetachFromParent(EAttachmentRule Rule = EAttachmentRule::KeepRelative);

	virtual FBox CalcLocalBounds() const { return FBox{ FVector(), FVector() }; }
	virtual FBox CalcBounds() const { return CalcLocalBounds().GetWorldAABB(GetWorldMatrix()); }

	FVector GetWorldLocation() const;
	FRotator GetWorldRotation() const;
	FVector GetWorldScale3D() const;
	FMatrix GetWorldMatrix() const;
	
	void SetWorldLocation(FVector NewLocation, bool bSweep);
	void SetWorldRotation(FRotator NewRotation, bool bSweep);
	void SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep);
	void SetWorldScale3D(FVector NewScale);	
	void SetWorldTransform(const FMatrix& InWorldMatrix, bool bSweep);

	// 자신을 충돌 검사 대상으로 이동할 때 호출한다.
	virtual bool MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit = nullptr);
	// 충돌 검사 대상을 지정해서 이동할 때 호출한다. nullptr이면 충돌 질의 없이 이동한다.
	bool MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit, USceneComponent* CollisionComponent);

	void MarkTransformDirty();
	virtual void OnTransformDirty() {};
	virtual void UpdateOverlaps(const TArray<FHitResult>& PendingOverlaps = {});

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
protected:
	virtual bool QueryMovement(const FVector& Start, const FVector& Delta, const FRotator& Rotation, FHitResult& OutHit, TArray<FHitResult>& OutOverlaps) { return true; }
	virtual void DispatchBlockingHit(const FHitResult& Hit) {}
private:
	// 충돌 질의 후 확정된 Transform만 적용하기 위해 분리했다. SetWorldLocationAndRotation을 호출하면 이동 처리에 재진입하므로,
	// 이 함수에서는 Transform과 관련 상태만 갱신하고 Overlap/Hit 처리는 MoveComponent에서 이어서 수행한다.
	void ApplyWorldLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation);
protected:
	bool bTransformDirty;
	FTransform Transform;

	USceneComponent* AttachParent = nullptr; // Attach 부모 정보
	TArray<USceneComponent*> AttachChildren;

	bool bAbsoluteLocation = false;
	bool bAbsoluteRotation = false;
	bool bAbsoluteScale = false;
};
