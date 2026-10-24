#include "EnginePCH.h"
#include "PrimitiveComponent.h"
#include "../Render/Renderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"


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
	if (GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		DestroyPhysicsState();
		return;
	}
	if (!IsRegistered() || !GetOwner() || !GetOwner()->GetWorld()) return;

	BodyInstance.InitBody(this);
	GetOwner()->GetWorld()->GetPhysicsScene().AddBody(&BodyInstance);
}

void UPrimitiveComponent::DestroyPhysicsState()
{
	BodyInstance.TerminateBody();
}

void UPrimitiveComponent::OnRegister()
{
	Super::OnRegister();
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
}

void UPrimitiveComponent::SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response)
{
	CollisionResponses.SetResponse(Channel, Response);
	BodyInstance.UpdateFilter();
}

void UPrimitiveComponent::SetCollisionResponseToAllChannels(ECollisionResponse Response)
{
	CollisionResponses.SetAllResponses(Response);
	BodyInstance.UpdateFilter();
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
			if (!CurrentHit.bBlockingHit) continue;
			MoveHit = CurrentHit;
			break;
		}
	}

	const FVector NewLocation = Start + Delta * MoveHit.Time;
	SetWorldLocationAndRotation(NewLocation, NewRotation, false);
	MoveHit.Location = GetWorldLocation();
	MoveHit.Distance = (MoveHit.Location - Start).Size();
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
