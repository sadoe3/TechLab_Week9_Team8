#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "GameFramework/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/MovementComponent.h"
#include "Component/TextRenderComponent.h"
#include "Math/Transform.h"
#include "Render/Renderer.h"
#include "Math/Frustum.h"
#include "Math/BVH.h"
#include "Engine/Scene.h"
#include "Collision/PhysicsScene.h"
#include "Camera/CameraActor.h"
#include "Asset/LOD/StaticMeshLODSelector.h"
#include "Engine/EngineTypes.h"
#include "Containers/Queue.h"
#include "Camera/CameraTypes.h"

//class ACameraActor;
class ULevel;
class UBillboardComponent;
class UEngine;
class FJsonArchive;
class FLuaRuntime;
class APlayerCameraManager;
struct FLODViewContext;
struct FCameraTransitionCurve;

struct FRenderStats
{
	uint32 TotalPrimitives = 0;
	uint32 VisiblePrimitives = 0;
	uint32 DrawCalls = 0;
	uint64 Triangles = 0;
	uint32 LODCounts[4] = {};
	uint64 LODTriangles[4] = {};

	void Reset() { *this = FRenderStats(); }
};

enum class EWorldPlayState : uint8
{
	NotStarted,
	Starting,
	Playing,
	Ending
};

class UWorld : public UObject
{
	DECLARE_CLASS(UWorld, UObject)

public:
	UWorld();
	virtual ~UWorld();

	bool Init();
	/*UPrimitiveComponent* SpawnPrimitive(FClass* Class);*/
	AActor* SpawnActor(UClass* Class, FName InName = NAME_None, const FTransform* Transform = nullptr);

	template <class T>
	T* SpawnActor(FName InName = NAME_None, const FTransform* Transform = nullptr)
	{
		return Cast<T>(SpawnActor(T::StaticClass(), InName, Transform));
	}

	void Tick(float DeltaTime);

	void GatherRenderPackets(FRenderQueue& RenderArray, const FLODViewContext* LODView = nullptr, const FFrustumPlanes* Frustum = nullptr, FRenderer* Renderer = nullptr);

	// 같은 World의 액터를 선택합니다. nullptr이면 Fallback으로 복귀하며 실패 시 기존 선택을 유지합니다.
	bool SetViewTarget(AActor* Actor, const FViewTargetTransitionParams& Params = {});
	// 사용자 시간 곡선을 사용하는 시점 전환
	bool SetViewTargetWithCurve(AActor* Actor, const FViewTargetTransitionParams& Params, const FCameraTransitionCurve& Curve);
	// 외부 시간축의 두 대상과 전환 경과 시간 전달
	bool SetViewTargetBlendAtTime(AActor* From, AActor* To, const FViewTargetTransitionParams& Params, float ElapsedTime);
	void SetCameraInputEnabled(bool bEnabled);

	// 현재 선택된 시점 제공 액터입니다.
	AActor* GetViewTarget() const;
	AActor* GetPendingViewTarget()const;
	APlayerCameraManager* GetPlayerCameraManager() const { return PlayerCameraManager; }

	// 선택된 액터의 첫 카메라 컴포넌트를 반환합니다.
	UCameraComponent* GetMainCamera() const;

	// Level
	ULevel* GetPersistentLevel() const { return PersistentLevel; }
	void SetPersistentLevel(ULevel* InLevel) { PersistentLevel = InLevel; }

	ULevel* GetCurrentLevel() const { return CurrentLevel; }
	void SetCurrentLevel(ULevel* InLevel) { CurrentLevel = InLevel; }

	int32 GetActorNum();

	bool DestroyActor(AActor* Actor);

	// View별 Billboard 행렬 공급자는 이 동기 호출 동안만 사용하며 저장하지 않는다.
	using FBillboardTraceTransform = FMatrix(*)(const UBillboardComponent&, const void*);
	// 현재 World의 Component에 Ray를 전달하고 가장 가까운 유효 교차를 반환한다.
	bool LineTraceSingle(const FRay& WorldRay, FHitResult& OutHit,
		FBillboardTraceTransform ResolveBillboard = nullptr, const void* ViewContext = nullptr);

	void BeginPlay();
	void EndPlay();

	void Shutdown();

	bool AcceptsChanges()const
	{
		return !bClearingWorld && !bShuttingDown && !bShutdown
			&& PlayState != EWorldPlayState::Ending;
	}

	// 게임월드 여부 판정. 플레이 시작에 사용
	bool IsGameWorld()const
	{
		return WorldType == EWorldType::Game ||
			WorldType == EWorldType::PIE;
	}

	// 시작했는가/초기화중인가
	bool IsRunningPlay()const 
	{
		return IsGameWorld()&& (PlayState == EWorldPlayState::Starting
			|| PlayState == EWorldPlayState::Playing);
	}

	// 시작했는가
	bool HasBegunPlay()const
	{
		return PlayState == EWorldPlayState::Playing;
	}

	// 틱 도중에 생성된 액터/컴포넌트들을 다음 프레임에 시작시키기 위한 함수
	void QueueActorForBeginPlay(AActor* Actor);
	void QueueComponentForBeginPlay(UActorComponent* Component);

	// 프레임 종료 시 삭제할 예약 수
	uint32 GetPendingDestroyCount() const
	{
		return PendingDestroyList.Num();
	}

	// 엔진 프레임 종료 전용 처리
	void FlushPendingDestruction(uint32 RequestCount);

	// 위치·회전은 world 기준이며 배율과 충돌 설정은 Component에서 가져온다.
	ECollisionQueryStatus QueryComponentOverlapMulti(const UPrimitiveComponent& Component, const FVector& Location, const FRotator& Rotation, TArray<FOverlapResult>& OutOverlaps, bool bIgnoreBlocks = false);
	ECollisionQueryStatus QueryComponentSweepMulti(const UPrimitiveComponent& Component, const FVector& Start, const FVector& Delta, const FRotator& Rotation, TArray<FHitResult>& OutHits, bool bIgnoreInitialBlockingHitsWhenMovingOut = false);

	FScene& GetScene() { return Scene; }
	FPhysicsScene& GetPhysicsScene() { return PhysicsScene; }
	const FPhysicsScene& GetPhysicsScene() const { return PhysicsScene; }
	FTickTaskManager& GetTickTaskManager() { return TickTaskManager; }
	FLuaRuntime* GetLuaRuntime() const { return LuaRuntime.get(); }
	const FRenderStats& GetRenderStats() const { return RenderStats; }

	void SetWorldType(EWorldType InType) { WorldType = InType; }
	EWorldType GetWorldType() const { return WorldType; }

	static UWorld* CreateWorld(const EWorldType InWorldType, bool bInformEngineOfWorld, FName WorldName = NAME_None); /*, UPackage* InWorldPackage = NULL, bool bAddToRoot = true, ERHIFeatureLevel::Type InFeatureLevel = ERHIFeatureLevel::Num, const InitializationValues* InIVS = nullptr, bool bInSkipInitWorld = false);*/

	bool IsPlayInEditor() const { return WorldType == EWorldType::PIE; }

	static UWorld* GetDuplicatedWorldForPIE(UWorld* InWorld);

	virtual void PostDuplicate(bool bDuplicateForPIE) override;


	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
private:
	friend class AActor;
	friend class UEngine;
	friend class FJsonArchive;

	// 초기 로드·프레임 경계·종료 전용 정리
	bool ClearWorld();

	// World가 생성하는 최종 복귀용 카메라를 준비합니다.
	void CreateFallbackCamera();

	// 같은 World의 액터에서 첫 카메라 컴포넌트를 찾습니다.
	UCameraComponent* FindCameraComponent(AActor* Actor) const;
	
	// 출발/도착 대상 입력 정책과 틱처리를 반영
	void SyncCameraTargets();

	EWorldType WorldType = EWorldType::None;

	struct alignas(64) FGatherChunk
	{
		TArray<FRenderPacket> Packets;              // 스태틱 묶음에 못 들어가는 것 (반투명 섹션, Renderer 없는 호출)
		TArray<uint32> SlowPathIndices;
		std::vector<FStaticDrawGroup> Groups;       // 불투명 스태틱 메시: (머티리얼, 메시, LOD)별 묶음. 항목만 매 프레임 비운다
		uint32 LastGroup = 0;                       // 바로 전 물체가 들어간 묶음 (연속한 물체는 대개 같은 묶음)
		uint32 StaticDrawCount = 0;
		uint32 LODCounts[4] = {};
		uint64 LODTriangles[4] = {};
	};

	TArray<FGatherChunk> GatherChunks;

	// 등록된 Tick 함수만 실행한다. Actor보다 먼저 사라져도 남은 함수와의 연결을 스스로 끊는다.
	FTickTaskManager TickTaskManager;

	// UUID로 액터/컴포넌트 관리
	TQueue<uint32> BeginPlayList;

	// World가 생성·보관하는 최종 복귀용 카메라 액터입니다.
	ACameraActor* FallbackCamera = nullptr;

	// Level 밖에서 World가 생성·해제하며 시점 선택과 최종 캐시를 관리합니다.
	APlayerCameraManager* PlayerCameraManager = nullptr;
	// TODO: PlayerController 도입 시 Viewport의 입력 허용과 플레이어 입력 제어 책임을 분리하고,
	// 이 상태와 SetCameraInputEnabled의 카메라 적용 처리를 World 밖으로 옮긴다. 현재는 대상 전환 간 입력 상태 보존용이다.
	bool bCameraInputEnabled = true;

	ULevel* PersistentLevel = nullptr;
	ULevel* CurrentLevel = nullptr;
	TArray<ULevel*> Levels;

	FScene Scene;
	FPhysicsScene PhysicsScene;

	// GatherRenderPackets가 매 프레임 채우는 컬링 결과. 용량을 재사용한다.
	TArray<FPrimitiveSceneProxy*> VisibleProxies;
	TArray<FLODSelectionInput> LODInputs;
	TArray<uint8> SelectedLODs;

	FRenderStats RenderStats;

	void StartPendingObjects();

	EWorldPlayState PlayState = EWorldPlayState::NotStarted;

	// 중복 정리 막는 변수
	bool bClearingWorld = false;
	bool bShuttingDown = false;
	bool bShutdown = false;

	// 삭제 전 외부 참조 정리
	void ReleaseExternalReferences();

	// 검증된 Component의 삭제 예약
	void QueueComponentForDestroy(UActorComponent* Component);

	// 실제 Actor 삭제
	void DestroyActorNow(AActor* Actor);

	// Actor·Component 삭제 식별자
	TQueue<uint32> PendingDestroyList;

	// 삭제 배치 재진입 방지
	bool bFlushingPendingDestroy = false;

	// World 전용 Lua VM 소유
	TUniquePtr<FLuaRuntime> LuaRuntime;

	// 대상 검증과 카메라 동기화의 공통 경로
	bool SetViewTargetInternal(AActor* Actor, const FViewTargetTransitionParams& Params, const FCameraTransitionCurve* Curve);
};
