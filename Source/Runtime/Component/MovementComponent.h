#pragma once
#include "ActorComponent.h"
#include "SceneComponent.h"
#include "PrimitiveComponent.h"
#include "UObject/WeakObjectPtr.h"

class UMovementComponent : public UActorComponent
{
	DECLARE_CLASS(UMovementComponent, UActorComponent)
	REFLECT_START(ClassName)
		PROPERTY(Velocity)
		REFLECT_END()

protected:
	TWeakObjectPtr<USceneComponent> UpdatedComponent;
	TWeakObjectPtr<UPrimitiveComponent> CollisionComponent;

public:
	FVector Velocity;
	
	float Gravity = 9.81f;

	// ActorComponent  Interface
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	// 이동 대상이 Shape면 자신, Mesh면 첫 번째 자식 Shape를 자동 선택한다. 없으면 nullptr.
	virtual void SetUpdatedComponent(USceneComponent* NewComponent);
	// 자동 선택 대신 사용할 Shape를 명시적으로 지정한다.
	bool SetCollisionComponent(UPrimitiveComponent* NewComponent);
	UPrimitiveComponent* GetCollisionComponent() const;

	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.0f, const FVector& MoveDelta = FVector::ZeroVector);
	virtual FVector ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& HIt) const;

	virtual bool MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit);
	// 일반 Block은 성공이며 OutHit로 확인한다. 질의/침투 해소 실패 또는 삭제 시 false.
	bool SafeMoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult& OutHit);
	virtual void StopMovementImmediately();

protected:
	UPrimitiveComponent* FindCollisionComponent() const;
	bool ResolvePenetration(const FHitResult& Hit, const FRotator& NewRotation);
	bool bConstrainToPlane = false;
	FVector PlaneConstraintNormal = FVector::ZeroVector;
	FVector PlaneConstraintOrigin = FVector::ZeroVector;

	virtual FVector ConstrainDirectionToPlane(FVector Direction) const;
};
