#include "EnginePCH.h"
#include "PrimitiveComponent.h"
#include "../Render/Renderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

#include "Component/ScriptComponent.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/Scene.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
	FString PrimitiveTypeToString(EPrimitiveType Type)
	{
		switch (Type)
		{
		case EPrimitiveType::Sphere:
			return "Sphere";
			break;
		case EPrimitiveType::Cube:
			return "Cube";
			break;
		case EPrimitiveType::Cone:
			return "Cone";
			break;
		case EPrimitiveType::Plane:
			return "Plane";
			break;
		default:
			return "";
			break;
		}
	}
}

UPrimitiveComponent::UPrimitiveComponent()
{
	//SetMaterial(UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial"));
}

UPrimitiveComponent::~UPrimitiveComponent()
{
	ClearOverlaps(false);
	BodyInstance.TerminateBody();
}

const FBodyInstance& UPrimitiveComponent::GetBodyInstance() const
{
	if (!BodyInstance.GetOwnerComponent())
		BodyInstance.InitBody(this);
	return BodyInstance;
}

void UPrimitiveComponent::CreatePhysicsState()
{
	if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || bDestroyingPhysicsState) return;
	if (GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		DestroyPhysicsState();
		return;
	}
	if (!IsRegistered() || !GetOwner() || !GetOwner()->GetWorld()) return;

	BodyInstance.InitBody(this);
	GetOwner()->GetWorld()->GetPhysicsScene().AddBody(&BodyInstance);
	UpdateOverlaps();
}

void UPrimitiveComponent::DestroyPhysicsState()
{
	if (bDestroyingPhysicsState) return;
	bDestroyingPhysicsState = true;
	BodyInstance.TerminateBody();
	ClearOverlaps();
	bDestroyingPhysicsState = false;
}

void UPrimitiveComponent::OnRegister()
{
	Super::OnRegister();
	// 이미 시작한 소유 스크립트 연결
	if (AActor* OwnerActor = GetOwner())
	{
		for (UActorComponent* Component : OwnerActor->GetComponents())
		{
			UScriptComponent* Script = Cast<UScriptComponent>(Component);
			if (Script && Script->HasBegunPlay())
			{
				Script->BindPrimitive(this);
			}
		}
	}

	CreatePhysicsState();
}

void UPrimitiveComponent::OnUnregister()
{
	DestroyPhysicsState();
	Super::OnUnregister();
}

void UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled NewEnabled)
{
	CollisionEnabled = static_cast<int32>(NewEnabled);
	CreatePhysicsState();
}

void UPrimitiveComponent::SetCollisionObjectType(ECollisionChannel NewType)
{
	ObjectType = static_cast<int32>(NewType);
	BodyInstance.UpdateFilter();
	UpdateOverlaps();
}

void UPrimitiveComponent::SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response)
{
	CollisionResponses.SetResponse(Channel, Response);
	BodyInstance.UpdateFilter();
	UpdateOverlaps();
}

void UPrimitiveComponent::SetCollisionResponseToAllChannels(ECollisionResponse Response)
{
	CollisionResponses.SetAllResponses(Response);
	BodyInstance.UpdateFilter();
	UpdateOverlaps();
}

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
}


void UPrimitiveComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue)
{
	//if (Mesh && Material)
	//{
	//	FRenderPacket rp;
	//	rp.mesh = Mesh;
	//	rp.material = Material;
	//	rp.model = GetWorldMatrix();

	//	// Todo: subuv
	//	//rp.bSubUV = false;

	//	RenderQueue.Enqueue(rp);
	//}
}

bool UPrimitiveComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
{
	const FStaticMeshData* Mesh = GetMeshData();
	return Mesh && TraceMesh(WorldRay, *Mesh, GetWorldMatrix(), OutHit);
}

void UPrimitiveComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "bVisible" || Property.Name == "bCastShadow" || Property.Name == "bReceiveShadow")
		MarkRenderStateDirty();
	if (Property.Name == "CollisionEnabled")
		CreatePhysicsState();
	else if (Property.Name == "ObjectType" || Property.Name.starts_with("CollisionResponses."))
		BodyInstance.UpdateFilter();
	if (Property.Name == "ObjectType" || Property.Name.starts_with("CollisionResponses.") || Property.Name == "bGenerateOverlapEvents")
		UpdateOverlaps();
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
	if (SceneProxy)
		SceneProxy->GetScene()->MarkRenderStateDirty(SceneProxy);
}

bool UPrimitiveComponent::LineTraceComponentLocal(const FRay& LocalRay, float& OutT)
{
	const FStaticMeshData* Mesh = GetMeshData();
	return Mesh && TraceMeshLocal(LocalRay, *Mesh, OutT);
}

void UPrimitiveComponent::OnTransformDirty()
{
	if (BodyInstance.GetOwnerComponent())
		BodyInstance.UpdateTransform();
	if (SceneProxy)
		SceneProxy->GetScene()->MarkDirty(SceneProxy);
}

bool UPrimitiveComponent::MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit)
{
	if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()))
	{
		if (Hit) *Hit = FHitResult();
		return false;
	}
	TArray<FHitResult> PendingOverlaps;
	const FVector Start = GetWorldLocation();
	FHitResult MoveHit;
	MoveHit.TraceStart = Start;
	MoveHit.TraceEnd = Start + Delta;
	MoveHit.Location = Start;
	MoveHit.Distance = 0.0f;
	if (Hit) *Hit = MoveHit;

	UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
	if (bSweep && World && GetCollisionEnabled() != ECollisionEnabled::NoCollision && GetCollisionShape().Type != ECollisionShapeType::None)
	{
		TArray<FHitResult> Hits;
		if (World->QueryComponentSweepMulti(*this, Start, Delta, NewRotation, Hits, true) != ECollisionQueryStatus::Success)
			return false;

		for (const FHitResult& CurrentHit : Hits)
		{
			if (!CurrentHit.bBlockingHit)
			{
				PendingOverlaps.Add(CurrentHit);
				continue;
			}
			MoveHit = CurrentHit;
			break;
		}
	}

	const FVector NewLocation = Start + Delta * MoveHit.Time;
	InternalSetWorldLocationAndRotation(NewLocation, NewRotation);
	MoveHit.Location = GetWorldLocation();
	MoveHit.Distance = (MoveHit.Location - Start).Size();
	UpdateOverlaps(PendingOverlaps);
	DispatchBlockingHit(MoveHit);
	if (MoveHit.HitComponent && (MoveHit.HitComponent->IsPendingDestroy() || (MoveHit.HitComponent->GetOwner() && MoveHit.HitComponent->GetOwner()->IsPendingDestroy()))) MoveHit.HitComponent = nullptr;
	if (Hit) *Hit = MoveHit;
	return true;
}

bool UPrimitiveComponent::TraceMesh(const FRay& WorldRay, const FStaticMeshData& Mesh, const FMatrix& WorldMatrix, FHitResult& OutResult)
{
	FBox MeshBox = Mesh.AABB.GetWorldAABB(WorldMatrix);
	float BoxT;
	if (!RayIntersectsAABB(WorldRay, MeshBox.Min, MeshBox.Max, BoxT))
	{
		return false;
	}

	FRay LocalRay = ToLocalRay(WorldRay, WorldMatrix);
	float T = FLT_MAX;

	if (!RayIntersectsMesh(LocalRay, Mesh, T)) return false;

	OutResult.HitComponent = this;
	OutResult.Distance = T;
	OutResult.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;

	return true;
}



bool UPrimitiveComponent::TraceMeshLocal(const FRay& LocalRay, const FStaticMeshData& Mesh, float& OutT)
{
	return RayIntersectsMesh(LocalRay, Mesh, OutT);
}

void UPrimitiveComponent::CreateRenderState()
{
	Super::CreateRenderState();
	GetOwner()->GetWorld()->GetScene().AddPrimitive(this);
}

void UPrimitiveComponent::DestroyRenderState()
{
	GetOwner()->GetWorld()->GetScene().RemovePrimitive(this);
	Super::DestroyRenderState();
}

void UPrimitiveComponent::SetGenerateOverlapEvents(bool bNewValue)
{
	bGenerateOverlapEvents = bNewValue;
	UpdateOverlaps();
}

bool UPrimitiveComponent::IsOverlappingComponent(const UPrimitiveComponent* Other) const
{
	for (const UPrimitiveComponent* CurrentComponent : OverlappingComponents)
		if (CurrentComponent == Other) return true;
	return false;
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
	for (const UPrimitiveComponent* CurrentComponent : OverlappingComponents)
		if (CurrentComponent->GetOwner() == Other) return true;
	return false;
}

bool UPrimitiveComponent::CanGenerateOverlap(const UPrimitiveComponent* Other) const
{
	return Other && Other != this && !(IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy())) && !(Other->IsPendingDestroy() || (Other->GetOwner() && Other->GetOwner()->IsPendingDestroy()))
		&& !bDestroyingPhysicsState && !Other->bDestroyingPhysicsState
		&& bGenerateOverlapEvents && Other->bGenerateOverlapEvents
		&& BodyInstance.GetPhysicsScene() && BodyInstance.GetPhysicsScene() == Other->BodyInstance.GetPhysicsScene()
		&& BodyInstance.IsQueryEnabled() && Other->BodyInstance.IsQueryEnabled()
		&& GetOwner() != Other->GetOwner()
		&& BodyInstance.GetFilterData().GetResponseTo(Other->BodyInstance.GetFilterData()) == ECollisionResponse::Overlap;
}

void UPrimitiveComponent::BeginComponentOverlap(UPrimitiveComponent* Other, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!CanGenerateOverlap(Other) || IsOverlappingComponent(Other)) return;
	OverlappingComponents.Add(Other);
	Other->OverlappingComponents.Add(this);
	NotifyBeginOverlap(Other, bFromSweep, SweepResult);
	// 첫 알림에서 삭제·이동·설정 변경으로 관계가 끝났으면 상대 Begin을 보내지 않는다.
	if (!CanGenerateOverlap(Other) || !IsOverlappingComponent(Other)) return;
	FHitResult OtherSweepResult = SweepResult;
	if (bFromSweep)
	{
		OtherSweepResult.HitComponent = this;
		OtherSweepResult.Normal = -SweepResult.Normal;
		OtherSweepResult.ImpactNormal = -SweepResult.ImpactNormal;
	}
	Other->NotifyBeginOverlap(this, bFromSweep, OtherSweepResult);
}

void UPrimitiveComponent::EndComponentOverlap(UPrimitiveComponent* Other, bool bNotify)
{
	const int32 CurrentIndex = OverlappingComponents.Find(Other);
	if (CurrentIndex == OverlappingComponents.Num()) return;
	OverlappingComponents.RemoveAt(CurrentIndex);
	const int32 OtherIndex = Other->OverlappingComponents.Find(this);
	if (OtherIndex < Other->OverlappingComponents.Num()) Other->OverlappingComponents.RemoveAt(OtherIndex);
	if (!bNotify) return;
	if (!(IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()))) NotifyEndOverlap(Other);
	if (!(Other->IsPendingDestroy() || (Other->GetOwner() && Other->GetOwner()->IsPendingDestroy())) && !IsOverlappingComponent(Other)) Other->NotifyEndOverlap(this);
}

void UPrimitiveComponent::ClearOverlaps(bool bNotify)
{
	const TArray<UPrimitiveComponent*> PreviousOverlaps = OverlappingComponents;
	for (UPrimitiveComponent* CurrentComponent : PreviousOverlaps)
		EndComponentOverlap(CurrentComponent, bNotify);
}

void UPrimitiveComponent::UpdateOverlaps(const TArray<FHitResult>& PendingOverlaps)
{
	if (bUpdatingOverlaps)
	{
		bOverlapUpdatePending = true;
		return;
	}

	bUpdatingOverlaps = true;
	bool bProcessPendingOverlaps = true;
	do
	{
		bOverlapUpdatePending = false;
		if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || bDestroyingPhysicsState || !bGenerateOverlapEvents || !BodyInstance.GetPhysicsScene() || !BodyInstance.IsQueryEnabled())
		{
			ClearOverlaps();
			continue;
		}

		UWorld* World = GetOwner()->GetWorld();
		TArray<FOverlapResult> FinalOverlaps;
		// 판정 실패를 이탈로 처리하지 않는다. 기존 목록을 유지한다.
		if (World->QueryComponentOverlapMulti(*this, GetWorldLocation(), GetWorldRotation(), FinalOverlaps, true) != ECollisionQueryStatus::Success)
			break;

		if (bProcessPendingOverlaps)
		{
			bProcessPendingOverlaps = false;
			for (const FHitResult& CurrentHit : PendingOverlaps)
			{
				if (!CurrentHit.bBlockingHit) BeginComponentOverlap(CurrentHit.HitComponent, true, CurrentHit);
				if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy())) break;
			}
			// 경로 Begin 콜백이 위치나 설정을 바꿨을 수 있으므로 최종 위치를 다시 확인한다.
			if (!PendingOverlaps.IsEmpty()) bOverlapUpdatePending = true;
			if (bOverlapUpdatePending) continue;
		}

		TArray<UPrimitiveComponent*> FinalComponents;
		for (const FOverlapResult& CurrentOverlap : FinalOverlaps)
			if (CanGenerateOverlap(CurrentOverlap.Component)) FinalComponents.Add(CurrentOverlap.Component);

		const TArray<UPrimitiveComponent*> PreviousOverlaps = OverlappingComponents;
		for (UPrimitiveComponent* CurrentComponent : PreviousOverlaps)
		{
			if (FinalComponents.Find(CurrentComponent) == FinalComponents.Num())
			{
				EndComponentOverlap(CurrentComponent, true);
				bOverlapUpdatePending = true;
				break;
			}
		}
		if (bOverlapUpdatePending || IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy())) continue;

		for (UPrimitiveComponent* CurrentComponent : FinalComponents)
		{
			if (IsOverlappingComponent(CurrentComponent)) continue;
			BeginComponentOverlap(CurrentComponent, false, FHitResult());
			// 알림에서 다른 Component도 이동하거나 설정이 바뀔 수 있다. 다음 관계 처리 전 다시 질의한다.
			bOverlapUpdatePending = true;
			break;
		}
	} while (bOverlapUpdatePending && !(IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy())));
	bUpdatingOverlaps = false;
	Super::UpdateOverlaps();
}

void UPrimitiveComponent::DispatchBlockingHit(const FHitResult& Hit)
{
	UPrimitiveComponent* OtherComponent = Hit.HitComponent;
	if (!Hit.bBlockingHit || IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || !OtherComponent || OtherComponent->IsPendingDestroy() || (OtherComponent->GetOwner() && OtherComponent->GetOwner()->IsPendingDestroy())) return;

	NotifyHit(OtherComponent, Hit);
	// 첫 알림에서 어느 한쪽이 삭제 요청되었으면 상대 알림을 보내지 않는다.
	if (IsPendingDestroy() || (GetOwner() && GetOwner()->IsPendingDestroy()) || OtherComponent->IsPendingDestroy() || (OtherComponent->GetOwner() && OtherComponent->GetOwner()->IsPendingDestroy())) return;

	FHitResult OtherHit = Hit;
	OtherHit.HitComponent = this;
	OtherHit.Normal = -Hit.Normal;
	OtherHit.ImpactNormal = -Hit.ImpactNormal;
	// 이동 정보는 원래 Sweep의 기록을 유지하고 상대와 법선만 수신자 기준으로 바꾼다.
	OtherComponent->NotifyHit(this, OtherHit);
}

void UPrimitiveComponent::NotifyHit(UPrimitiveComponent* OtherComponent, const FHitResult& Hit)
{
	HTR_LOG(Info, "OnComponentHit: {} [{}] -> {} [{}], Time={}, StartPenetrating={}", GetName(), GetUUID(), OtherComponent->GetName(), OtherComponent->GetUUID(), Hit.Time, Hit.bStartPenetrating);

	OnComponentHit.Broadcast(this, OtherComponent, Hit);
}

void UPrimitiveComponent::NotifyBeginOverlap(UPrimitiveComponent* Other, bool bFromSweep, const FHitResult& SweepResult)
{
	HTR_LOG(Info, "OnComponentBeginOverlap: {} [{}] -> {} [{}], FromSweep={}", GetName(), GetUUID(), Other->GetName(), Other->GetUUID(), bFromSweep);

	OnComponentBeginOverlap.Broadcast(this, Other, bFromSweep, SweepResult);
}

void UPrimitiveComponent::NotifyEndOverlap(UPrimitiveComponent* Other)
{
	HTR_LOG(Info, "OnComponentEndOverlap: {} [{}] -> {} [{}]", GetName(), GetUUID(), Other->GetName(), Other->GetUUID());
	OnComponentEndOverlap.Broadcast(this, Other);
}
