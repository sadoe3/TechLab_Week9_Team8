#include "EnginePCH.h"
#include "MovementComponent.h"
#include "GameFramework/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Engine/World.h"

void UMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (AActor* MyActor = GetOwner())
	{
		if (USceneComponent* NewUpdatedCompoent = MyActor->GetRootComponent())
		{
			SetUpdatedComponent(NewUpdatedCompoent);
		}
	}

	PrimaryComponentTick.bCanEverTick = true;
}

void UMovementComponent::BeginPlay()
{
	Super::BeginPlay();
}
  
void UMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewComponent)
{
	UpdatedComponent = NewComponent;
}

void UMovementComponent::UpdateComponentVelocity()
{
	if (UpdatedComponent)
	{
		UpdatedComponent->ComponentVelocity = Velocity;
	}
}

void UMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{

}

FVector UMovementComponent::ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& HIt) const
{
	if (!bConstrainToPlane)
	{
		return FVector::VectorPlaneProject(Delta, Normal) * Time;		
	}
	else
	{
		const FVector ProjectedNormal = ConstrainDirectionToPlane(Normal);
		return FVector::VectorPlaneProject(Delta, ProjectedNormal) * Time;
	}
}

bool UMovementComponent::MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit)
{
	if (Hit) *Hit = FHitResult();
	if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || !UpdatedComponent) return false;
	if (UpdatedComponent->IsPendingDestroy() || (UpdatedComponent->GetOwner() && UpdatedComponent->GetOwner()->IsPendingDestroy()))
	{
		UpdatedComponent = nullptr;
		return false;
	}

	USceneComponent* MovedComponent = UpdatedComponent.Get();
	const bool bMoved = MovedComponent->MoveComponent(Delta, NewRotation, bSweep, Hit);
	if ((MovedComponent->IsPendingDestroy() || (MovedComponent->GetOwner() && MovedComponent->GetOwner()->IsPendingDestroy())) && UpdatedComponent.Get() == MovedComponent) UpdatedComponent = nullptr;
	return bMoved;
}

bool UMovementComponent::SafeMoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult& OutHit)
{
	USceneComponent* MovedComponent = UpdatedComponent.Get();
	if (!MoveUpdatedComponent(Delta, NewRotation, bSweep, &OutHit)) return false;
	if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || !UpdatedComponent || UpdatedComponent.Get() != MovedComponent) return false;
	if (!bSweep || !OutHit.bStartPenetrating) return true;

	if (ResolvePenetration(OutHit, NewRotation))
	{
		if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || UpdatedComponent.Get() != MovedComponent) return false;
		const bool bMoved = MoveUpdatedComponent(Delta, NewRotation, true, &OutHit);
		return bMoved && !IsPendingDestroy() && !(GetOwner() && GetOwner()->IsPendingDestroy()) && UpdatedComponent && UpdatedComponent.Get() == MovedComponent;
	}
	if (OutHit.HitComponent && (OutHit.HitComponent->IsPendingDestroy() || (OutHit.HitComponent->GetOwner() && OutHit.HitComponent->GetOwner()->IsPendingDestroy()))) OutHit.HitComponent = nullptr;
	return false;
}

bool UMovementComponent::ResolvePenetration(const FHitResult& Hit, const FRotator& NewRotation)
{
	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(UpdatedComponent.Get());
	UWorld* World = Primitive && Primitive->GetOwner() ? Primitive->GetOwner()->GetWorld() : nullptr;
	if (!World) return false;

	constexpr int32 MaxPenetrationIterations = 4;
	constexpr float PenetrationPullbackDistance = 0.001f;
	FHitResult PenetrationHit = Hit;
	for (int32 CurrentIteration = 0; CurrentIteration < MaxPenetrationIterations; ++CurrentIteration)
	{
		const FVector Adjustment = ConstrainDirectionToPlane(PenetrationHit.Normal * (PenetrationHit.PenetrationDepth + PenetrationPullbackDistance));
		if (Adjustment.SizeSquared() <= SMALL_NUMBER * SMALL_NUMBER) return false;
		FHitResult AdjustmentHit;
		if (!MoveUpdatedComponent(Adjustment, NewRotation, true, &AdjustmentHit)) return false;
		if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || UpdatedComponent.Get() != Primitive || Primitive->IsPendingDestroy()) return false;

		// 보정 위치가 비었는지 먼저 확인한다. Sweep은 첫 Block 하나만 반환한다.
		TArray<FOverlapResult> Overlaps;
		if (World->QueryComponentOverlapMulti(*Primitive, Primitive->GetWorldLocation(), NewRotation, Overlaps) != ECollisionQueryStatus::Success) return false;
		bool bHasBlockingOverlap = false;
		for (const FOverlapResult& CurrentOverlap : Overlaps)
			if (CurrentOverlap.bBlockingHit) bHasBlockingOverlap = true;
		if (!bHasBlockingOverlap) return true;

		// 영 이동 질의에서 다음 보정에 사용할 법선과 깊이를 얻는다.
		TArray<FHitResult> Hits;
		if (World->QueryComponentSweepMulti(*Primitive, Primitive->GetWorldLocation(), FVector::ZeroVector, NewRotation, Hits) != ECollisionQueryStatus::Success) return false;
		bool bFoundPenetration = false;
		for (const FHitResult& CurrentHit : Hits)
		{
			if (!CurrentHit.bBlockingHit || !CurrentHit.bStartPenetrating) continue;
			PenetrationHit = CurrentHit;
			bFoundPenetration = true;
			break;
		}
		if (!bFoundPenetration) return false;
	}
	return false;
}

void UMovementComponent::StopMovementImmediately()
{
	Velocity = FVector::ZeroVector;
	UpdateComponentVelocity();
}

FVector UMovementComponent::ConstrainDirectionToPlane(FVector Direction) const
{
	if (bConstrainToPlane)
	{
		Direction = FVector::VectorPlaneProject(Direction, PlaneConstraintNormal);
	}

	return Direction;
}
