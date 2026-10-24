#pragma once

#include "SceneComponent.h"
#include "../Render/Shader.h"
#include "../Render/Mesh.h"
#include "Render/RenderPacket.h"
#include "Render/GeometryGenerator.h"
#include "Collision/HitResult.h"
#include "Collision/BodyInstance.h"

class FPrimitiveSceneProxy;

enum class EPrimitiveType
{
	Sphere,
	Cube,
	Cone,
	Plane,

	Particle,
	ParticleQuad
};

class UPrimitiveComponent :public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	// Material은 여기서 등록하지 않는다. 컴포넌트마다 머티리얼을 다루는 방식이 달라서
	// (빌보드는 단일 머티리얼, 스태틱 메시는 슬롯별 덮어쓰기) 각자 등록한다.
	REFLECT_START(ClassName)
		PROPERTY(bVisible)
		PROPERTY(bCastShadow)
		PROPERTY(bReceiveShadow)
		PROPERTY(CollisionEnabled)
		PROPERTY(ObjectType)
		PROPERTY(CollisionResponses.WorldStatic)
		PROPERTY(CollisionResponses.WorldDynamic)
		PROPERTY(CollisionResponses.Pawn)
	REFLECT_END()
public:
	UPrimitiveComponent();
	virtual ~UPrimitiveComponent();

	virtual void BeginPlay() override;
	virtual FCollisionLocalShape GetCollisionShape() const { return {}; }
	const FBodyInstance& GetBodyInstance() const;
	void CreatePhysicsState();
	void DestroyPhysicsState();
	ECollisionEnabled GetCollisionEnabled() const { return CollisionEnabled == static_cast<int32>(ECollisionEnabled::QueryOnly) ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision; }
	ECollisionChannel GetCollisionObjectType() const { return static_cast<ECollisionChannel>(ObjectType); }
	const FCollisionResponseContainer& GetCollisionResponses() const { return CollisionResponses; }
	void SetCollisionEnabled(ECollisionEnabled NewEnabled);
	void SetCollisionObjectType(ECollisionChannel NewType);
	void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response);
	void SetCollisionResponseToAllChannels(ECollisionResponse Response);

	// Todo: subuv
	//void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue);
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue);

	virtual const FStaticMeshData* GetMeshData() const { return nullptr; }

	// Outline처럼 컴포넌트를 통째로 한 번 더 그릴 때 쓰는 GPU 메시.
	// 메시가 없거나(텍스트) View에 따라 형상이 정해지는 컴포넌트(빌보드·파티클)는
	// 컴포넌트 월드 행렬만으로 같은 그림을 못 만들므로 nullptr을 돌려준다.
	virtual UStaticMesh* GetRenderMesh() const { return nullptr; }

	virtual int32 GetNumMaterials() const { return 0; }
	virtual UMaterial* GetMaterial(int32 SlotIndex) const { return nullptr; }
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) {}
	//
	// FShader* GetShader() const { return Shader.get(); };

	bool CastsShadow() const { return bCastShadow; }
	void SetCastShadow(bool bValue) { bCastShadow = bValue; MarkRenderStateDirty(); }
	bool ReceivesShadow() const { return bReceiveShadow; }
	void SetReceiveShadow(bool bValue) { bReceiveShadow = bValue; MarkRenderStateDirty(); }
	virtual void PostEditChangeProperty(const FProperty& Property) override;

	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible)
	{
		bVisible = bInVisible;
		MarkRenderStateDirty();
	}

	virtual bool LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit);
	virtual bool LineTraceComponentLocal(const FRay& LocalRay, float& OutT);
	virtual FBox CalcLocalBounds() const override
	{
		const FStaticMeshData* Data = GetMeshData();
		return Data ? Data->AABB : Super::CalcLocalBounds();
	}

	FPrimitiveSceneProxy* GetSceneProxy() const { return SceneProxy; }

	void MarkRenderStateDirty();

	virtual void OnTransformDirty() override;

	// Delta/NewRotation은 world 기준. 질의 실패 시 false와 위치 유지, 막힘 여부는 Hit로 반환한다.
	virtual bool MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit = nullptr) override;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	bool TraceMesh(const FRay& WorldRay, const FStaticMeshData& Mesh, const FMatrix& WorldMatrix, FHitResult& OutResult);
	bool TraceMeshLocal(const FRay& LocalRay, const FStaticMeshData& Mesh, float& OutT);
	bool bVisible = true;
	bool bCastShadow = true;
	bool bReceiveShadow = true;
	// 등록 책임을 프리미티브로 옮김
	virtual void CreateRenderState() override;
	virtual void DestroyRenderState() override;
	// Reflection 저장형은 int32, 호출부는 ECollisionEnabled/ECollisionChannel을 사용한다.
	int32 CollisionEnabled = static_cast<int32>(ECollisionEnabled::NoCollision);
	int32 ObjectType = static_cast<int32>(ECollisionChannel::WorldDynamic);
	FCollisionResponseContainer CollisionResponses;

private:
	friend class FScene;
	FPrimitiveSceneProxy* SceneProxy = nullptr;
	mutable FBodyInstance BodyInstance;
};
