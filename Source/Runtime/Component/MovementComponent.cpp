#include "EnginePCH.h"
#include "MovementComponent.h"
#include "GameFramework/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ShapeComponent.h"
#include "Component/MeshComponent.h"
#include "Engine/World.h"

void UMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (AActor* MyActor = GetOwner(); MyActor && !UpdatedComponent)
	{
		if (USceneComponent* NewUpdatedCompoent = MyActor->GetRootComponent())
		{
			SetUpdatedComponent(NewUpdatedCompoent);
		}
	}

	// 생성자에서 이동 대상을 지정한 뒤 추가된 자식 Shape도 초기화 시 연결한다.
	if (!CollisionComponent && !CollisionComponent.IsStale()) CollisionComponent = FindCollisionComponent();
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
	if (NewComponent && UpdatedComponent.Get() == NewComponent) return;
	UpdatedComponent = NewComponent;
	CollisionComponent = FindCollisionComponent();
}

UPrimitiveComponent* UMovementComponent::FindCollisionComponent() const
{
	if (UShapeComponent* Shape = Cast<UShapeComponent>(UpdatedComponent.Get())) return Shape;
	if (UMeshComponent* Mesh = Cast<UMeshComponent>(UpdatedComponent.Get()))
	{
		for (USceneComponent* CurrentChild : Mesh->GetAttachChildren())
		{
			UShapeComponent* Shape = Cast<UShapeComponent>(CurrentChild);
			if (Shape && !Shape->IsPendingDestroy() && Shape->GetOwner() == Mesh->GetOwner()) return Shape;
		}
	}
	return nullptr;
}

bool UMovementComponent::SetCollisionComponent(UPrimitiveComponent* NewComponent)
{
	if (!Cast<UShapeComponent>(NewComponent) || !UpdatedComponent || NewComponent->GetOwner() != UpdatedComponent->GetOwner()) return false;
	USceneComponent* CurrentAncestor = NewComponent;
	while (CurrentAncestor && CurrentAncestor != UpdatedComponent.Get()) CurrentAncestor = CurrentAncestor->GetAttachParent();
	if (!CurrentAncestor) return false;
	CollisionComponent = NewComponent;
	return true;
}

UPrimitiveComponent* UMovementComponent::GetCollisionComponent() const
{
	return CollisionComponent.Get();
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
	if (CollisionComponent.IsStale()) return false;
	UPrimitiveComponent* Collision = GetCollisionComponent();
	// Shape 루트는 자신, Mesh 루트는 자동 선택한 자식 Shape를 전달한다. 없으면 무충돌 이동한다.
	const bool bMoved = MovedComponent->MoveComponent(Delta, NewRotation, bSweep, Hit, Collision);
	if ((MovedComponent->IsPendingDestroy() || (MovedComponent->GetOwner() && MovedComponent->GetOwner()->IsPendingDestroy())) && UpdatedComponent.Get() == MovedComponent) UpdatedComponent = nullptr;
	return bMoved;
}

bool UMovementComponent::SafeMoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult& OutHit)
{
	USceneComponent* MovedComponent = UpdatedComponent.Get();
	UPrimitiveComponent* Collision = GetCollisionComponent();
	if (!MoveUpdatedComponent(Delta, NewRotation, bSweep, &OutHit)) return false;
	if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || !UpdatedComponent || UpdatedComponent.Get() != MovedComponent || GetCollisionComponent() != Collision || (Collision && Collision->IsPendingDestroy())) return false;
	if (!bSweep || !OutHit.bStartPenetrating) return true;

	if (ResolvePenetration(OutHit, NewRotation))
	{
		if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || UpdatedComponent.Get() != MovedComponent) return false;
		const bool bMoved = MoveUpdatedComponent(Delta, NewRotation, true, &OutHit);
		return bMoved && !IsPendingDestroy() && !(GetOwner() && GetOwner()->IsPendingDestroy()) && UpdatedComponent && UpdatedComponent.Get() == MovedComponent && GetCollisionComponent() == Collision && (!Collision || !Collision->IsPendingDestroy());
	}
	if (OutHit.HitComponent && (OutHit.HitComponent->IsPendingDestroy() || (OutHit.HitComponent->GetOwner() && OutHit.HitComponent->GetOwner()->IsPendingDestroy()))) OutHit.HitComponent = nullptr;
	return false;
}

bool UMovementComponent::ResolvePenetration(const FHitResult& Hit, const FRotator& NewRotation)
{
	UPrimitiveComponent* Primitive = GetCollisionComponent();
	USceneComponent* MovedComponent = UpdatedComponent.Get();
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
		if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || UpdatedComponent.Get() != MovedComponent || GetCollisionComponent() != Primitive || Primitive->IsPendingDestroy()) return false;

		// 보정 위치가 비었는지 먼저 확인한다. Sweep은 첫 Block 하나만 반환한다.
		TArray<FOverlapResult> Overlaps;
		if (World->QueryComponentOverlapMulti(*Primitive, Primitive->GetWorldLocation(), Primitive->GetWorldRotation(), Overlaps) != ECollisionQueryStatus::Success) return false;
		bool bHasBlockingOverlap = false;
		for (const FOverlapResult& CurrentOverlap : Overlaps)
			if (CurrentOverlap.bBlockingHit) bHasBlockingOverlap = true;
		if (!bHasBlockingOverlap) return true;

		// 영 이동 질의에서 다음 보정에 사용할 법선과 깊이를 얻는다.
		TArray<FHitResult> Hits;
		if (World->QueryComponentSweepMulti(*Primitive, Primitive->GetWorldLocation(), FVector::ZeroVector, Primitive->GetWorldRotation(), Hits) != ECollisionQueryStatus::Success) return false;
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
}

FVector UMovementComponent::ConstrainDirectionToPlane(FVector Direction) const
{
	if (bConstrainToPlane)
	{
		Direction = FVector::VectorPlaneProject(Direction, PlaneConstraintNormal);
	}

	return Direction;
}
