#include "EnginePCH.h"
#include "Component/SceneComponent.h"

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
	const FVector WorldLocation = AttachParent ? AttachParent->GetWorldMatrix().TransformPosition(NewLocation) : NewLocation;
	const FRotator WorldRotation = AttachParent ? (AttachParent->GetWorldRotation().Quaternion() * NewRotation.Quaternion()).ToFRotator() : NewRotation;
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

FRotator USceneComponent::GetWorldRotation() const
{
	if (AttachParent)
	{
		FQuat ParentQuat = AttachParent->GetWorldRotation().Quaternion();
		FQuat LocalQuat = Transform.GetOrientation();

		return (ParentQuat * LocalQuat).ToFRotator();
	}

	return Transform.Rotation;
}

FVector USceneComponent::GetWorldLocation() const
{
	FMatrix WorldMatrix = GetWorldMatrix();

	return FVector(WorldMatrix[3][0], WorldMatrix[3][1], WorldMatrix[3][2]);
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

	const FVector WorldLocation = 
		AttachParent->GetWorldMatrix().TransformPosition(Transform.Location);

	const FQuat WorldRotation = 
		AttachParent->GetWorldRotation().Quaternion() * Transform.GetOrientation();

	const FVector WorldScale =
		Transform.Scale * AttachParent->GetWorldScale3D();

	return FTransform(WorldRotation.ToFRotator(), WorldLocation, WorldScale).GetLocalMatrix();
}

void USceneComponent::SetWorldTransform(const FMatrix& InWorldMatrix, bool bSweep)
{
	FMatrix LocalMatrix = InWorldMatrix;
	if (AttachParent)
	{
		FMatrix ParentWorldMatrix = AttachParent->GetWorldMatrix();
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
	const FVector NewLocation = AttachParent ? AttachParent->GetWorldMatrix().TransformPosition(NewRelativeLocation) : NewRelativeLocation;
	const FRotator NewRotation = AttachParent ? (AttachParent->GetWorldRotation().Quaternion() * NewRelativeRotation.Quaternion()).ToFRotator() : NewRelativeRotation;
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

void USceneComponent::InternalSetWorldLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
{
	Transform.Location = AttachParent ? AttachParent->GetWorldMatrix().Inverse().TransformPosition(NewLocation) : NewLocation;
	Transform.Rotation = AttachParent ? (AttachParent->GetWorldRotation().Quaternion().Inverse() * NewRotation.Quaternion()).ToFRotator() : NewRotation;
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
	if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()))
	{
		if (Hit) *Hit = FHitResult();
		return false;
	}
	const FVector Start = GetWorldLocation();
	InternalSetWorldLocationAndRotation(Start + Delta, NewRotation);
	UpdateOverlaps();
	if (Hit)
	{
		*Hit = FHitResult();
		Hit->TraceStart = Start;
		Hit->TraceEnd = Hit->Location = GetWorldLocation();
		Hit->Distance = (Hit->Location - Start).Size();
	}
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


