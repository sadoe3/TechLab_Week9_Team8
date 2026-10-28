#include "EnginePCH.h"
#include "Component/SceneComponent.h"
#include "Component/PrimitiveComponent.h"

#include "GameFramework/Actor.h"

USceneComponent::~USceneComponent()
{
	if (AttachParent)
	{
		const int32 ChildIndex = AttachParent->AttachChildren.Find(this);
		if (ChildIndex < AttachParent->AttachChildren.Num()) AttachParent->AttachChildren.RemoveAt(ChildIndex);
	}
	for (USceneComponent* CurrentChild : AttachChildren)
		CurrentChild->AttachParent = nullptr;
}

void USceneComponent::SetRelativeLocation(const FVector& InLocation)
{
	SetRelativeLocationAndRotation(InLocation, Transform.Rotation);
}

void USceneComponent::SetRelativeRotation(const FRotator& InRotation)
{
	SetRelativeLocationAndRotation(Transform.Location, InRotation);
}

void USceneComponent::SetRelativeLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
{
	FVector WorldLocation = NewLocation;
	FRotator WorldRotation = NewRotation;
	if (AttachParent)
	{
		const FQuat ParentRotation = AttachParent->GetRotationForAttachment().Quaternion();
		WorldLocation = AttachParent->GetLocationForAttachment()
			+ ParentRotation.RotateVector(AttachParent->GetWorldScale3D() * NewLocation);
		WorldRotation = (ParentRotation * NewRotation.Quaternion()).ToFRotator();
	}
	MoveComponent(WorldLocation - GetWorldLocation(), WorldRotation, false);
}

void USceneComponent::SetRelativeScale3D(const FVector& InScale)
{
	Transform.Scale = InScale;
	MarkTransformDirty();
	UpdateOverlaps();
}

void USceneComponent::SetTransform(const FTransform& InTransform)
{
	const FTransform NewTransform = InTransform;
	Transform.Scale = NewTransform.Scale;
	SetRelativeLocationAndRotation(NewTransform.Location, NewTransform.Rotation);
}

void USceneComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "Transform")
	{
		MarkTransformDirty();
		UpdateOverlaps();
	}
}

void USceneComponent::SetupAttachment(USceneComponent* InParent, EAttachmentRule Rule)
{
	if (InParent == this || AttachParent == InParent) return;
	for (USceneComponent* CurrentParent = InParent; CurrentParent; CurrentParent = CurrentParent->AttachParent)
		if (CurrentParent == this) return;

	const FMatrix PreviousWorldMatrix = GetWorldMatrix();
	if (AttachParent)
	{
		const int32 ChildIndex = AttachParent->AttachChildren.Find(this);
		if (ChildIndex < AttachParent->AttachChildren.Num()) AttachParent->AttachChildren.RemoveAt(ChildIndex);
	}
	AttachParent = InParent;
	if (AttachParent) AttachParent->AttachChildren.Add(this);

	if (Rule == EAttachmentRule::KeepWorld)
		SetWorldTransform(PreviousWorldMatrix, false);
	else if (Rule == EAttachmentRule::SnapToTarget)
		SetTransform(FTransform::Identity);
	else
	{
		MarkTransformDirty();
		UpdateOverlaps();
	}
}

void USceneComponent::DetachFromParent(EAttachmentRule Rule)
{
	if (!AttachParent) return;
	const FMatrix PreviousWorldMatrix = GetWorldMatrix();
	const int32 ChildIndex = AttachParent->AttachChildren.Find(this);
	if (ChildIndex < AttachParent->AttachChildren.Num()) AttachParent->AttachChildren.RemoveAt(ChildIndex);
	AttachParent = nullptr;

	if (Rule == EAttachmentRule::KeepWorld)
		SetWorldTransform(PreviousWorldMatrix, false);
	else
	{
		MarkTransformDirty();
		UpdateOverlaps();
	}
}

FTransform USceneComponent::GetTransformForAttachment() const
{
	return FTransform(GetRotationForAttachment(), GetLocationForAttachment(), GetWorldScale3D());
}

FRotator USceneComponent::GetRotationForAttachment() const
{
	return GetWorldRotation();
}

FVector USceneComponent::GetLocationForAttachment() const
{
	return GetWorldLocation();
}

FRotator USceneComponent::GetWorldRotation() const
{
	if (AttachParent)
	{
		FQuat ParentQuat = AttachParent->GetRotationForAttachment().Quaternion();
		FQuat LocalQuat = Transform.GetOrientation();

		return (ParentQuat * LocalQuat).ToFRotator();
	}

	return Transform.Rotation;
}

FVector USceneComponent::GetWorldLocation() const
{
	if (!AttachParent) return Transform.Location;
	return AttachParent->GetLocationForAttachment()
		+ AttachParent->GetRotationForAttachment().Quaternion().RotateVector(
			AttachParent->GetWorldScale3D() * Transform.Location);
}

FVector USceneComponent::GetWorldScale3D() const
{
	if (AttachParent)
	{
		FVector ParentScale = AttachParent->GetWorldScale3D();

		return FVector(
			Transform.Scale.X * ParentScale.X,
			Transform.Scale.Y * ParentScale.Y,
			Transform.Scale.Z * ParentScale.Z
		);
	}

	return Transform.Scale;
}

FMatrix USceneComponent::GetWorldMatrix() const
{
	if (!AttachParent)
	{
		return Transform.GetLocalMatrix();
	}

	const FQuat ParentRotation = AttachParent->GetRotationForAttachment().Quaternion();
	const FVector ParentScale = AttachParent->GetWorldScale3D();
	const FVector WorldLocation = AttachParent->GetLocationForAttachment()
		+ ParentRotation.RotateVector(ParentScale * Transform.Location);
	const FQuat WorldRotation = ParentRotation * Transform.GetOrientation();
	const FVector WorldScale = Transform.Scale * ParentScale;

	return FTransform(WorldRotation.ToFRotator(), WorldLocation, WorldScale).GetLocalMatrix();
}

void USceneComponent::SetWorldTransform(const FMatrix& InWorldMatrix, bool bSweep)
{
	// 행렬 역변환에는 모든 성분이 필요하다. 이후 재합성에서도 같은 부모 변환을 재사용한다.
	const FTransform Parent = AttachParent ? AttachParent->GetTransformForAttachment() : FTransform::Identity;
	const FQuat ParentRotation = Parent.GetOrientation();
	FMatrix LocalMatrix = InWorldMatrix;
	if (AttachParent)
	{
		FMatrix ParentWorldMatrix = Parent.GetLocalMatrix();
		LocalMatrix = InWorldMatrix * ParentWorldMatrix.Inverse();
	}

	const FVector NewRelativeLocation(LocalMatrix[3][0], LocalMatrix[3][1], LocalMatrix[3][2]);

	FVector XAxis(LocalMatrix[0][0], LocalMatrix[0][1], LocalMatrix[0][2]);
	FVector YAxis(LocalMatrix[1][0], LocalMatrix[1][1], LocalMatrix[1][2]);
	FVector ZAxis(LocalMatrix[2][0], LocalMatrix[2][1], LocalMatrix[2][2]);

	const float XAxisSize = XAxis.Size();
	const float YAxisSize = YAxis.Size();
	const float ZAxisSize = ZAxis.Size();

	const FVector PreviousScale = Transform.Scale;
	Transform.Scale = FVector(XAxisSize, YAxisSize, ZAxisSize);

	XAxis = XAxis.Normalized();
	YAxis = YAxis.Normalized();
	ZAxis = ZAxis.Normalized();

	FMatrix RotationMatrix = FMatrix::Identity;
	RotationMatrix.SetAxis(0, XAxis);
	RotationMatrix.SetAxis(1, YAxis);
	RotationMatrix.SetAxis(2, ZAxis);

	const FRotator NewRelativeRotation = MatrixToRotator(RotationMatrix);
		
	// Sweep에는 새 배율의 Body가 필요하다. 이 단계에서는 Overlap 알림을 보내지 않는다.
	MarkTransformDirty();
	const FVector NewLocation = AttachParent ? Parent.Location + ParentRotation.RotateVector(Parent.Scale * NewRelativeLocation) : NewRelativeLocation;
	const FRotator NewRotation = AttachParent ? (ParentRotation * NewRelativeRotation.Quaternion()).ToFRotator() : NewRelativeRotation;
	if (!MoveComponent(NewLocation - GetWorldLocation(), NewRotation, bSweep))
	{
		Transform.Scale = PreviousScale;
		MarkTransformDirty();
	}
}

void USceneComponent::SetWorldLocation(FVector NewLocation, bool bSweep)
{
	MoveComponent(NewLocation - GetWorldLocation(), GetWorldRotation(), bSweep);
}

void USceneComponent::SetWorldRotation(FRotator NewRotation, bool bSweep)
{
	MoveComponent(FVector::ZeroVector, NewRotation, bSweep);
}

void USceneComponent::SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep)
{
	MoveComponent(NewLocation - GetWorldLocation(), NewRotation, bSweep);
}

void USceneComponent::ApplyWorldLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
{
	if (AttachParent)
	{
		const FTransform Parent = AttachParent->GetTransformForAttachment();
		Transform.Location = Parent.GetLocalMatrix().Inverse().TransformPosition(NewLocation);
		Transform.Rotation = (Parent.GetOrientation().Inverse() * NewRotation.Quaternion()).ToFRotator();
	}
	else
	{
		Transform.Location = NewLocation;
		Transform.Rotation = NewRotation;
	}
	MarkTransformDirty();
}

void USceneComponent::SetWorldScale3D(FVector NewScale)
{
	FVector RelativeScale = NewScale;
	if (AttachParent)
	{
		const FVector ParentScale = AttachParent->GetWorldScale3D();
		for (int32 CurrentAxis = 0; CurrentAxis < 3; ++CurrentAxis)
			RelativeScale[CurrentAxis] = ParentScale[CurrentAxis] != 0.0f ? NewScale[CurrentAxis] / ParentScale[CurrentAxis] : 0.0f;
	}
	SetRelativeScale3D(RelativeScale);
}

bool USceneComponent::MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit)
{
	// Capsule 루트처럼 자신이 충돌 대상인 일반 이동도 공통 실행 경로를 사용한다.
	return MoveComponent(Delta, NewRotation, bSweep, Hit, this);
}

bool USceneComponent::MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit, USceneComponent* CollisionComponent)
{
	if (Hit) *Hit = FHitResult();
	if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy())) return false;
	if (CollisionComponent)
	{
		if (CollisionComponent->IsPendingDestroy() || CollisionComponent->GetOwner() != GetOwner()) return false;
		USceneComponent* CurrentAncestor = CollisionComponent;
		while (CurrentAncestor && CurrentAncestor != this) CurrentAncestor = CurrentAncestor->GetAttachParent();
		if (!CurrentAncestor) return false;
	}

	const FVector MovementStart = GetWorldLocation();
	const FQuat RotationDelta = NewRotation.Quaternion() * GetWorldRotation().Quaternion().Inverse();
	const FVector QueryStart = CollisionComponent ? MovementStart + RotationDelta.RotateVector(CollisionComponent->GetWorldLocation() - MovementStart) : MovementStart;
	const FRotator QueryRotation = CollisionComponent ? (RotationDelta * CollisionComponent->GetWorldRotation().Quaternion()).ToFRotator() : NewRotation;
	FHitResult MoveHit;
	MoveHit.TraceStart = MoveHit.Location = QueryStart;
	MoveHit.TraceEnd = QueryStart + Delta;
	MoveHit.Distance = 0.0f;
	if (Hit) *Hit = MoveHit;

	TArray<FHitResult> PendingOverlaps;
	if (bSweep && CollisionComponent && !CollisionComponent->QueryMovement(QueryStart, Delta, QueryRotation, MoveHit, PendingOverlaps)) return false;

	// 질의 -> 자기 Transform 적용 -> Overlap 갱신 -> Hit 알림 순서를 여기서 관리한다.
	ApplyWorldLocationAndRotation(MovementStart + Delta * MoveHit.Time, NewRotation);
	MoveHit.Location = CollisionComponent ? CollisionComponent->GetWorldLocation() : GetWorldLocation();
	MoveHit.Distance = (MoveHit.Location - QueryStart).Size();
	if (CollisionComponent) CollisionComponent->UpdateOverlaps(PendingOverlaps);
	if (CollisionComponent != this) UpdateOverlaps();
	if (!IsPendingDestroy() && CollisionComponent) CollisionComponent->DispatchBlockingHit(MoveHit);
	if (MoveHit.HitComponent && (MoveHit.HitComponent->IsPendingDestroy() || (MoveHit.HitComponent->GetOwner() && MoveHit.HitComponent->GetOwner()->IsPendingDestroy()))) MoveHit.HitComponent = nullptr;
	if (Hit) *Hit = MoveHit;
	return true;
}
void USceneComponent::UpdateOverlaps(const TArray<FHitResult>& PendingOverlaps)
{
	if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy())) return;
	// 자식은 부모와 함께 이동한 최종 위치만 검사한다. 부모의 Sweep 결과는 전달하지 않는다.
	const TArray<USceneComponent*> Children = AttachChildren;
	for (USceneComponent* CurrentChild : Children)
	{
		if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy())) break;
		if (!(CurrentChild->IsPendingDestroy() || (CurrentChild->GetOwner() && CurrentChild->GetOwner()->IsPendingDestroy())) && CurrentChild->GetAttachParent() == this) CurrentChild->UpdateOverlaps();
	}
}

void USceneComponent::MarkTransformDirty()
{
	if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy())) return;
	OnTransformDirty();         

	const TArray<USceneComponent*> Children = AttachChildren;
	for (USceneComponent* CurrentChild : Children)
		if (!(CurrentChild->IsPendingDestroy() || (CurrentChild->GetOwner() && CurrentChild->GetOwner()->IsPendingDestroy()))) CurrentChild->MarkTransformDirty();
}

void USceneComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);   // UActorComponent: Owner까지 처리됨

	FArchive& Ar = Record.GetUnderlyingArchive();
	if (Ar.HasAnyPortFlags(EPropertyPortFlags::PPF_Duplicate))
	{
		UObject* ParentObj = AttachParent;
		Record << SA_VALUE("AttachParent", ParentObj);
		AttachParent = static_cast<USceneComponent*>(ParentObj);

		int32 Num = AttachChildren.Num();
		FStructuredArchive::FArray ChildArray = Record.EnterArray("AttachChildren", Num);
		if (Ar.IsLoading())
			AttachChildren.SetNum(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			UObject* Obj = AttachChildren[i];
			ChildArray.EnterElement() << Obj;
			AttachChildren[i] = static_cast<USceneComponent*>(Obj);
		}
	}
}


