#include "EnginePCH.h"
#include "World.h"
#include "Level.h"

#include "Core/EngineStatics.h"
#include "GameFramework/Actor/StaticMeshActor.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Input/InputSystem.h"

#include "UObject/UObjectIterator.h"

#include "Collision/Ray.h"
#include "Component/BillboardComponent.h"

#include "Component/StaticMeshComponent.h"
#include "Asset/LOD/StaticMeshLODSelector.h"

#include "Math/Frustum.h"

#include "Core/Stats/LightweightStats.h"
#include "Core/Stats/EditorStats.h"
#include "Core/Async/TaskPool.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "Engine/Engine.h"
#include "Scripting/LuaRuntime.h"

DECLARE_CYCLE_STAT("Actor Tick", STAT_ActorTick); // Actor 틱 측정
DECLARE_CYCLE_STAT("Update All Transforms", STAT_UpdateAllTransforms); // 각 Transform의 Update 시간 측정
DECLARE_CYCLE_STAT("Gather Render Packets", STAT_GatherRenderPackets);
DECLARE_CYCLE_STAT("Frustum Cull", STAT_FrustumCull);
DECLARE_CYCLE_STAT("Gather Elements", STAT_GatherElements);
DECLARE_CYCLE_STAT("Gather - LOD", STAT_GatherLOD);
DECLARE_CYCLE_STAT("Gather - Submit", STAT_GatherSubmit);

UWorld::UWorld() = default;

UWorld::~UWorld()
{
	Shutdown();
}

bool UWorld::Init()
{
	// Spawn Actor로 카메라 생성하고 세팅하기
	PersistentLevel = NewObject<ULevel>(this);

	if (!PersistentLevel)
	{
		HTR_LOG(Error, "Failed to create PersistentLevel");
		return false;
	}

	//레벨 연결
	PersistentLevel->SetWorld(this);
	Levels.Add(PersistentLevel);
	CurrentLevel = PersistentLevel;

	//카메라 생성
	CreateFallbackCamera();

	return true;
}

AActor* UWorld::SpawnActor(UClass* Class, FName InName, const FTransform* Transform)
{
	if (!AcceptsChanges()){ return nullptr; }
	if (!Class) return nullptr;
	if (!Class->IsChildOf(AActor::StaticClass())) return nullptr;

	// 0. 중복 이름 처리
	if (!InName.IsNone() && StaticFindObjectFast(nullptr, PersistentLevel, InName))
	{
		InName = MakeUniqueObjectName(PersistentLevel, Class, InName);
	}

	// 1. ObjectFactory로 Actor 생성
	AActor* NewActor = NewObject<AActor>(PersistentLevel, Class, InName);

	// 2. Actor에 World/Level 연결
	NewActor->World = this;
	NewActor->Level = PersistentLevel;

	// 3. Transform 적용
	const FTransform SpawnTransform = Transform ? *Transform : FTransform::Identity;

	if (NewActor->GetRootComponent())
	{
		NewActor->GetRootComponent()->SetTransform(SpawnTransform);
	}

	for (UActorComponent* Component : NewActor->GetComponents())
	{
		Component->RegisterComponent();
	}

	// 4. Level->Actors에 등록
	PersistentLevel->AddActor(NewActor);

	// 5. PlayList에 추가
	QueueActorForBeginPlay(NewActor);

	return NewActor;
}

void UWorld::Tick(float DeltaTime)
{
	if (bFlushingPendingDestroy || !AcceptsChanges())
	{
		return;
	}
	if (HasBegunPlay())
	{
		StartPendingObjects();
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateAllTransforms);
		Scene.UpdateAllTransforms();
	}

	if (!IsGameWorld() || !HasBegunPlay())
	{
		return;
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ActorTick);
		// 모든 Actor를 도는 대신 등록된 Tick 함수(메인 카메라 포함)만 실행한다.
		TickTaskManager.RunAllTickGroups(DeltaTime);
	}
}

bool UWorld::ClearWorld()
{
	// 삭제 배치·정리·종료 중 재진입 차단
	if (bFlushingPendingDestroy || bClearingWorld || bShutdown || PlayState == EWorldPlayState::Ending)
	{
		return false;
	}

	bClearingWorld = true;
	ReleaseExternalReferences();
	BeginPlayList.Reset();
	PendingDestroyList.Reset();
	// Level 액터를 삭제하기 전에 시점을 내장 카메라로 돌립니다.
	SetViewTarget(nullptr);

	TArray<AActor*> ClearingActors;

	for (ULevel* Level : Levels)
	{
		if (!Level)
		{
			continue;
		}

		for (AActor* Actor : Level->GetActors())
		{
			if (Actor)
			{
				ClearingActors.Add(Actor);
			}
		}
	}

	// Level 액터 종료와 초기화 해제
	for (AActor* Actor : ClearingActors)
	{
		Actor->DispatchEndPlay();
	}

	for (AActor* Actor : ClearingActors)
	{
		Actor->UninitializeComponents();
	}

	// 렌더 등록 일괄 제거
	Scene.RemoveAllPrimitives();
	Scene.RemoveAllFogInfo();
	Scene.RemoveAllLights();

	for (ULevel* Level : Levels)
	{
		if (Level)
		{
			Level->ClearActors();
		}
	}

	bClearingWorld = false;

	// Fallback Tick 복구
	if (FallbackCamera)
	{
		FallbackCamera->RegisterAllActorTickFunctions(MainViewTarget == FallbackCamera);
	}

	HTR_LOG(Info, "{} : ", GetActorNum());
	return true;
}

void UWorld::GatherRenderPackets(FRenderQueue& RenderQueue, const FLODViewContext* LODView, const FFrustumPlanes* Frustum, FRenderer* Renderer)
{
	// 멤버로 두어 매 프레임 용량을 재사용한다.
	RenderStats.Reset();
	VisibleProxies.Reset();

	{
		SCOPE_CYCLE_COUNTER(STAT_FrustumCull);
		// 컬링 단계에서는 프록시 포인터만 모으고, 컴포넌트 역참조(가시성 확인)는 어차피 컴포넌트를 읽는 Gather로 미룬다.
		const auto Visit = [&](FPrimitiveSceneProxy* Proxy) { VisibleProxies.Add(Proxy); };

		if (Frustum)
		{
			Scene.BVH.QueryCull(
				FrustumAllPlanesMask,
				[Frustum](const FBox& Bounds, uint32& Mask)
				{
					return static_cast<EBVHCullResult>(ClassifyBoxInFrustum(Bounds, *Frustum, Mask));
				},
				Visit);
		}
		else
		{
			Scene.BVH.QueryCull(0, [](const FBox&, uint32&) { return EBVHCullResult::Inside; }, Visit);
		}
	}

	RenderStats.TotalPrimitives = Scene.Proxies.Num();

	// GPU 오클루전: 프러스텀을 통과한 물체를 GPU에서 가림 판정하고 결과를 이번 프레임에 받아 온다.
	// Cull이 켜져 있으면 가려진 물체를 목록에서 빼서 이후 Gather·정렬·드로우를 모두 건너뛴다.
	// 꺼져 있으면(검증 모드) 목록은 그대로 두고 패킷에 판정만 표시한다.
	const uint8* OccludedMask = nullptr;
	if (Renderer && LODView && Renderer->GetGPUOcclusion().GetSettings().bEnabled)
	{
		FGPUOcclusion& Occlusion = Renderer->GetGPUOcclusion();
		if (Occlusion.Run(VisibleProxies.GetData(), VisibleProxies.Num(), *LODView))
		{
			const std::vector<uint8>& Occluded = Occlusion.GetOccluded();
			if (Occlusion.GetSettings().bCull)
			{
				uint32 Kept = 0;
				for (uint32 i = 0; i < VisibleProxies.Num(); ++i)
					if (!Occluded[i])
						VisibleProxies[Kept++] = VisibleProxies[i];
				VisibleProxies.SetNum(Kept);
			}
			else
			{
				OccludedMask = Occluded.data();
			}
		}
	}

	RenderStats.VisiblePrimitives = VisibleProxies.Num();

	constexpr uint32 ExtraSlots = 4096;
	const uint32 VisibleCount = VisibleProxies.Num();
	const uint32 MaxSlots = VisibleCount + ExtraSlots;
	uint8* SlotDest = Renderer ? Renderer->BeginObjectConstants(MaxSlots) : nullptr;
	uint32 NextExtraSlot = VisibleCount;

	// 조각 수 = 스레드 수 × 4. 잘게 나눠야 먼저 끝난 스레드가 남은 조각을 가져가서 부하가 고르게 된다.
	FTaskPool& Pool = FTaskPool::Get();
	const uint32 ChunkCount = FMath::Clamp(VisibleCount, 1u, Pool.GetNumThreads() * 4);

	if (GatherChunks.Num() < ChunkCount)
		GatherChunks.SetNum(ChunkCount);     // 늘릴 때만. 줄이지 않아야 배열 용량이 계속 재사용된다.

	for (uint32 c = 0; c < GatherChunks.Num(); ++c)   // 이번에 안 쓰는 조각의 묶음도 비워 둔다 (Renderer에 넘기지 않도록)
	{
		FGatherChunk& Chunk = GatherChunks[c];
		Chunk.Packets.Reset();               // 용량은 유지, 개수만 0
		Chunk.SlowPathIndices.Reset();
		std::fill(std::begin(Chunk.LODCounts), std::end(Chunk.LODCounts), 0u);
		std::fill(std::begin(Chunk.LODTriangles), std::end(Chunk.LODTriangles), 0ull);
		for (FStaticDrawGroup& Group : Chunk.Groups)
			Group.Items.clear();             // 용량은 유지
		Chunk.StaticDrawCount = 0;
	}

	// Renderer가 있으면 불투명 스태틱 메시는 패킷 대신 조각별 묶음에 작은 항목으로 넣는다.
	// 동일 바인딩의 개별 드로우를 묶어 패킷 복사와 항목별 정렬을 줄인다.
	const bool bStaticGroups = Renderer != nullptr;
	if (Renderer)
		Renderer->ResetStaticDrawGroups();

	{
		SCOPE_CYCLE_COUNTER(STAT_GatherElements);

		LODInputs.Reset();
		if (LODView)
		{
			LODInputs.Reserve(VisibleProxies.Num());
			for (const FPrimitiveSceneProxy* Proxy : VisibleProxies)
				LODInputs.Add({ Proxy->GetLODSphere(), Proxy->GetRenderState() });
			SelectLODs(LODInputs, *LODView, SelectedLODs);
		}
		Pool.ParallelFor(VisibleCount, ChunkCount, [&](uint32 Begin, uint32 End, uint32 ChunkIndex)
			{
				FGatherChunk& Out = GatherChunks[ChunkIndex];      // 이 조각 전용. 다른 스레드는 절대 안 건드림
				// (머티리얼, 메시, LOD) 묶음 찾기. 조합이 몇 개뿐이라 선형 탐색이면 충분하고, 바로 전 묶음을 먼저 본다.
				const auto FindGroup = [&Out](UMaterial* Material, UStaticMesh* Mesh, uint8 LOD) -> FStaticDrawGroup&
					{
						if (Out.LastGroup < Out.Groups.size())
						{
							FStaticDrawGroup& Last = Out.Groups[Out.LastGroup];
							if (Last.Material == Material && Last.Mesh == Mesh && Last.LODIndex == LOD)
								return Last;
						}
						for (uint32 g = 0; g < Out.Groups.size(); ++g)
						{
							FStaticDrawGroup& Group = Out.Groups[g];
							if (Group.Material == Material && Group.Mesh == Mesh && Group.LODIndex == LOD)
							{
								Out.LastGroup = g;
								return Group;
							}
						}
						Out.LastGroup = static_cast<uint32>(Out.Groups.size());
						FStaticDrawGroup& Group = Out.Groups.emplace_back();
						Group.Material = Material;
						Group.Mesh = Mesh;
						Group.LODIndex = LOD;
						return Group;
					};

				for (uint32 VisibleIndex = Begin; VisibleIndex < End; ++VisibleIndex)
				{
					FPrimitiveSceneProxy* Proxy = VisibleProxies[VisibleIndex];   // 읽기만
					if (!Proxy->IsVisible()) continue;

					UStaticMesh* Mesh = Proxy->GetMesh();
					if (!Mesh)
					{
						Out.SlowPathIndices.Add(VisibleIndex);     // 컴포넌트를 건드리는 경로는 메인이 나중에
						continue;
					}

					const uint32 LOD = LODView ? SelectedLODs[VisibleIndex] : 0;
					const FCachedMeshLOD& CachedLOD = Proxy->GetLOD(LOD);
					++Out.LODCounts[LOD];                          // RenderStats 대신 조각 전용 통계

					uint32 Slot = InvalidObjectSlot;
					if (SlotDest)
					{
						// 칸 VisibleIndex는 이 반복만 쓴다 → 스레드끼리 겹치지 않음
						WriteObjectSlot(SlotDest, VisibleIndex, Proxy->GetLocalToWorld(), Proxy->GetNormalMatrix(), Proxy->ReceivesShadow());
						Slot = VisibleIndex;
					}

					const bool bOccludedByGpu = OccludedMask && OccludedMask[VisibleIndex];
					for (uint32 i = 0; i < CachedLOD.NumSections; ++i)
					{
						const FCachedMeshSection& Section = Proxy->GetSection(CachedLOD.FirstSection + i);
						Out.LODTriangles[LOD] += Section.IndexCount / 3;

						if (bStaticGroups && Section.Material && Section.Material->BlendState == EBlendState::Opaque)
						{
							FStaticDrawGroup& Group = FindGroup(Section.Material, Mesh, static_cast<uint8>(LOD));
							Group.Items.push_back({ Proxy, Slot, Section.StartIndex, Section.IndexCount, bOccludedByGpu ? 1u : 0u });
							++Out.StaticDrawCount;
							continue;
						}

						FRenderPacket& Packet = Out.Packets.AddDefaulted_GetRef();   // 조각 전용 배열에 추가
						Packet.Proxy = Proxy;
						Packet.Mesh = Mesh;
						Packet.Material = Section.Material;
						Packet.StartIndex = Section.StartIndex;
						Packet.IndexCount = Section.IndexCount;
						Packet.LODIndex = static_cast<uint8>(LOD);
						Packet.Slot = Slot;
						Packet.bOccludedByGpu = bOccludedByGpu;
					}
				}
			});

		// 묶음은 World 메모리 그대로 Renderer에 넘긴다 (복사 없음). 정렬은 Renderer가 묶음 단위로 한다.
		uint32 StaticDraws = 0;
		if (Renderer)
		{
			for (uint32 c = 0; c < ChunkCount; ++c)
			{
				StaticDraws += GatherChunks[c].StaticDrawCount;
				for (const FStaticDrawGroup& Group : GatherChunks[c].Groups)
					if (!Group.Items.empty())
						Renderer->AddStaticDrawGroup(&Group);
			}
		}

		uint32 TotalPackets = 0;
		for (uint32 c = 0; c < ChunkCount; ++c)
			TotalPackets += GatherChunks[c].Packets.Num();

		RenderQueue.Reserve(TotalPackets + 256);             // 느린 경로 몫 약간 여유
		for (uint32 c = 0; c < ChunkCount; ++c)
			if (GatherChunks[c].Packets.Num() > 0)
				RenderQueue.Append(GatherChunks[c].Packets);  // 조각 순서대로 이어 붙이기

		for (uint32 c = 0; c < ChunkCount; ++c)
		{
			for (uint32 VisibleIndex : GatherChunks[c].SlowPathIndices)
			{
				FPrimitiveSceneProxy* Proxy = VisibleProxies[VisibleIndex];
				UPrimitiveComponent* Primitive = Proxy->GetComponent();
				if (!Primitive || !Primitive->IsVisible())
					continue;

				const uint32 FirstNew = RenderQueue.Num();

				// 프록시 캐시가 없는 스태틱 메시는 기존처럼 LOD를 골라 제출하고, 그 외는 컴포넌트에 맡긴다.
				UStaticMeshComponent* StaticMeshComponent = LODView ? Cast<UStaticMeshComponent>(Primitive) : nullptr;
				if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
				{
					const uint32 LOD = SelectedLODs[VisibleIndex];
					StaticMeshComponent->SubmitToRenderQueue(RenderQueue, LOD);
				}
				else
				{
					Primitive->SubmitToRenderQueue(RenderQueue);
				}

				// continue 없이 항상 여기까지 와서 새 패킷에 여유 칸을 배정한다.
				for (uint32 p = FirstNew; p < RenderQueue.Num(); ++p)
				{
					if (!SlotDest || NextExtraSlot >= MaxSlots) break;
					FRenderPacket& Packet = RenderQueue[p];
					if (Packet.Proxy)
					{
						WriteObjectSlot(SlotDest, NextExtraSlot, Packet.Proxy->GetLocalToWorld(), Packet.Proxy->GetNormalMatrix(), Packet.Proxy->ReceivesShadow());
					}
					else
					{
						// 프록시 없는 패킷(빌보드 등)은 캐시가 없으므로 여기서 계산한다.
						const FMatrix& Model = Packet.Model ? *Packet.Model : FMatrix::Identity;
						WriteObjectSlot(SlotDest, NextExtraSlot, Model, MakeNormalMatrix(Model));
					}
					Packet.Slot = NextExtraSlot++;
				}
			}
		}

		for (uint32 c = 0; c < ChunkCount; ++c)
			for (uint32 L = 0; L < 4; ++L)
			{
				RenderStats.LODCounts[L] += GatherChunks[c].LODCounts[L];
				RenderStats.LODTriangles[L] += GatherChunks[c].LODTriangles[L];
			}

		if (SlotDest) Renderer->EndObjectConstants();
		RenderStats.DrawCalls = RenderQueue.Num() + StaticDraws;
		for (uint64 T : RenderStats.LODTriangles) RenderStats.Triangles += T;
	}
}

//void UWorld::GatherRenderPackets(TArray<FRenderPacket>& RenderArray, const FLODViewContext* LODView, const FFrustumPlanes* Frustum)
//{
//	for (TObjectIterator<UPrimitiveComponent> Itr; Itr; ++Itr)
//	{
//		if (!*Itr || !Itr->IsVisible())
//			continue;
//
//		if (Frustum)
//		{
//			const FBox Box = Itr->CalcBounds();
//			const FAABB Bounds{
//				(Box.Min + Box.Max) * 0.5f,
//				(Box.Max - Box.Min) * 0.5f
//			};
//			if (!IsAABBInFrustum(Bounds, *Frustum))
//				continue;
//		}
//
//		if (LODView)
//		{
//			if (auto* Component = Cast<UStaticMeshComponent>(*Itr))
//			{
//				if (UStaticMesh* Mesh =
//					Component->GetStaticMesh())
//				{
//					const uint32 LOD = SelectStaticMeshLOD(
//						*Mesh,
//						Component->GetWorldMatrix(),
//						*LODView);
//
//					Component->SubmitToRenderQueue(RenderQueue, LOD);
//					continue;
//				}
//			}
//		}
//
//		Itr->SubmitToRenderQueue(RenderQueue);
//	}
//}

void UWorld::CreateFallbackCamera()
{
	// 시점 선택이 바뀌어도 내장 카메라는 중복 생성하지 않습니다.
	if (FallbackCamera)
		return;

	FallbackCamera = NewObject<ACameraActor>(this);

	if (!FallbackCamera)
	{
		HTR_LOG(Error, "Failed to create FallbackCamera");
		return;
	}

	FallbackCamera->World = this;
	FallbackCamera->Level = nullptr;
	for (UActorComponent* Component : FallbackCamera->GetComponents())
	{
		if (Component)
		{
			Component->RegisterComponent();
		}
	}
	FallbackCamera->GetCameraComponent()->SetRelativeLocation(FVector(-5.0f, -5.0f, 5.0f));

	// 최초에는 내장 카메라를 현재 시점으로 선택합니다.
	SetViewTarget(nullptr);
}

UCameraComponent* UWorld::FindCameraComponent(AActor* Actor) const
{
	// 선택 대상 검증은 이 함수에 모읍니다.
	if (!Actor || Actor->GetWorld() != this)
	{
		return nullptr;
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
		{
			return Camera;
		}
	}

	return nullptr;
}

bool UWorld::SetViewTarget(AActor* Actor)
{
	if (Actor && (!AcceptsChanges() || Actor->IsPendingDestroy()))
	{
		return false;
	}
	// nullptr은 내장 카메라로 복귀하는 명시적인 요청입니다.
	AActor* NewViewTarget = Actor ? Actor : FallbackCamera;

	// 카메라가 없으면 현재 선택과 Tick 등록 상태를 유지합니다.
	UCameraComponent* NewCamera = FindCameraComponent(NewViewTarget);
	if (!NewCamera || NewCamera->IsPendingDestroy())
		return false;

	if (MainViewTarget == NewViewTarget)
		return true;

	// 전환 직후에도 기존 뷰포트의 입력 차단 상태를 유지합니다.
	if (UCameraComponent* PreviousCamera = GetMainCamera())
		NewCamera->SetExternalInputManaged(PreviousCamera->IsExternalInputManaged());

	MainViewTarget = NewViewTarget;

	// Level 밖의 내장 카메라만 선택 여부에 따라 Tick을 등록·해제합니다.
	// 배치 액터의 Tick 등록은 기존 Actor 생명주기가 관리합니다.
	if (FallbackCamera)
		FallbackCamera->RegisterAllActorTickFunctions(MainViewTarget == FallbackCamera);

	return true;
}

UCameraComponent* UWorld::GetMainCamera() const
{
	// 컴포넌트 포인터를 따로 보관하지 않고 현재 액터에서 조회합니다.
	return FindCameraComponent(MainViewTarget);
}

int32 UWorld::GetActorNum()
{
	return PersistentLevel ? PersistentLevel->GetActorNum() : 0;
}

bool UWorld::DestroyActor(AActor* Actor)
{
	// Level 소속 Actor의 삭제 요청 검증
	if (!AcceptsChanges() || !Actor || Actor->GetWorld() != this || !Actor->GetLevel())
	{
		return false;
	}

	// 중복 요청의 성공 처리
	if (Actor->bPendingDestroy)
	{
		return true;
	}

	Actor->bPendingDestroy = true;
	PendingDestroyList.Enqueue(Actor->GetUUID());

	return true;
}

void UWorld::DestroyActorNow(AActor* Actor)
{
	ULevel* Level = Actor->GetLevel();

	// 외부 참조 해제와 종료
	ReleaseExternalReferences();
	Actor->DispatchEndPlay();
	Actor->UninitializeComponents();

	// 삭제 전 시점 복구
	if (MainViewTarget == Actor)
	{
		SetViewTarget(nullptr);
	}

	// Level 소유 목록 제거
	for (int32 Index = Level->Actors.Num() - 1; Index >= 0; --Index)
	{
		if (Level->Actors[Index] == Actor)
		{
			Level->Actors.RemoveAt(Index, 1);
			break;
		}
	}

	const FString ActorName = Actor->GetName();
	const uint32 ActorUUID = Actor->GetUUID();

	// Tick·소유 Component 해제
	Actor->RegisterAllActorTickFunctions(false);
	delete Actor;

	HTR_LOG(Info, "Destroy Actor : {} UUID {}", ActorName, ActorUUID);
}

void UWorld::FlushPendingDestruction(uint32 RequestCount)
{
	const uint32 Count = std::min(RequestCount, PendingDestroyList.Num());

	if (Count == 0 || bFlushingPendingDestroy || !AcceptsChanges())
	{
		return;
	}

	bFlushingPendingDestroy = true;

	// 확정한 요청만 처리
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const uint32 UUID = PendingDestroyList.Peek();
		PendingDestroyList.Dequeue();

		UObject* Object = FindObjectByUUID(UUID);

		if (AActor* Actor = Cast<AActor>(Object))
		{
			if (Actor->GetWorld() == this && Actor->IsPendingDestroy())
			{
				DestroyActorNow(Actor);
			}
		}
		else if (UActorComponent* Component = Cast<UActorComponent>(Object))
		{
			AActor* Owner = Component->GetOwner();

			// Owner 삭제 예약은 Actor 경로에서 일괄 처리
			if (Owner && Owner->GetWorld() == this && !Owner->IsPendingDestroy() && Component->IsPendingDestroy())
			{
				Owner->DestroyComponentNow(Component);
			}
		}
	}

	// 삭제 배치의 공간 인덱스 변경 반영
	if (Scene.bElementListChanged)
	{
		Scene.BuildBVH();
		Scene.bElementListChanged = false;
	}

	bFlushingPendingDestroy = false;
}

void UWorld::QueueComponentForDestroy(UActorComponent* Component)
{
	// 호출자가 검증한 Component의 삭제 예약
	Component->bPendingDestroy = true;
	PendingDestroyList.Enqueue(Component->GetUUID());
}

// 다른 World의 객체를 제외하고 Component 교차 중 최근접 결과를 선택한다.
bool UWorld::LineTraceSingle(const FRay& WorldRay, FHitResult& OutHit,
	FBillboardTraceTransform ResolveBillboard, const void* ViewContext)
{
	SCOPE_CYCLE_COUNTER_ALWAYS(EditorStats::STAT_PickingTime_Name);
	OutHit = FHitResult();
	float NearestT = std::numeric_limits<float>::max();

	const auto TraceComponent = [&](FPrimitiveSceneProxy* Proxy, float& InOutNearestT)
		{
			if (UStaticMesh* Mesh = Proxy ? Proxy->GetMesh() : nullptr)
			{
				if (!Proxy->IsVisible())
					return false;

				const FMatrix& WorldToLocal = Proxy->GetWorldToLocal();
				const FRay LocalRay{
					.Origin = WorldToLocal.TransformPosition(WorldRay.Origin),
					.Direction = WorldToLocal.TransformVector(WorldRay.Direction)
				};

				float T = InOutNearestT;
				if (!RayIntersectsMesh(LocalRay, Mesh->GetMeshData(), T))
					return false;

				OutHit.HitComponent = Proxy->GetComponent();
				OutHit.Distance = T;
				OutHit.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;
				InOutNearestT = T;
				return true;
			}

			UPrimitiveComponent* Component = Proxy ? Proxy->GetComponent() : nullptr;

			if (!Component || !Component->IsVisible())
				return false;

			if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
			{
				if (!ResolveBillboard)
				{
					FHitResult Hit;
					if (!Billboard->LineTraceComponent(WorldRay, Hit) ||
						Hit.Distance >= InOutNearestT)
					{
						return false;
					}

					OutHit = Hit;
					InOutNearestT = Hit.Distance;
					return true;
				}

				const FMatrix BillboardToWorld = ResolveBillboard(*Billboard, ViewContext);

				const FRay LocalRay = ToLocalRay(WorldRay, BillboardToWorld);

				float T = InOutNearestT;
				if (!Billboard->LineTraceComponentLocal(LocalRay, T))
				{
					return false;
				}

				OutHit.HitComponent = Billboard;
				OutHit.Distance = T;
				OutHit.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;
				InOutNearestT = T;
				return true;
			}

			const FMatrix& WorldToLocal = Proxy->GetWorldToLocal();
			const FRay LocalRay{
				.Origin = WorldToLocal.TransformPosition(WorldRay.Origin),
				.Direction = WorldToLocal.TransformVector(WorldRay.Direction)
			};

			float T = InOutNearestT;
			if (!Component->LineTraceComponentLocal(LocalRay, T))
			{
				return false;
			}

			OutHit.HitComponent = Component;
			OutHit.Distance = T;
			OutHit.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;
			InOutNearestT = T;
			return true;
		};

	if (Scene.bElementListChanged)
	{
		for (FPrimitiveSceneProxy* Proxy : Scene.Proxies)
		{
			TraceComponent(Proxy, NearestT);
		}
	}
	else
	{
		const FPreparedRay PreparedRay(WorldRay);

		Scene.BVH.TraceClosest(
			[&](const FBox& Bounds, float& OutEnterT) { return RayIntersectsAABB(PreparedRay, Bounds.Min, Bounds.Max, OutEnterT); },
			[&](FPrimitiveSceneProxy* Proxy, float& OutNearestT) { return TraceComponent(Proxy, OutNearestT); },
			NearestT);
	}
	return OutHit.HitComponent != nullptr;
}

void UWorld::BeginPlay()
{
	if (bFlushingPendingDestroy || !AcceptsChanges() || !IsGameWorld() || PlayState != EWorldPlayState::NotStarted)
	{
		return;
	}

	// 최초 게임 시작의 Lua VM 생성
	if (!LuaRuntime)
	{
		LuaRuntime = MakeUnique<FLuaRuntime>();
	}

	PlayState = EWorldPlayState::Starting;

	QueueActorForBeginPlay(FallbackCamera);

	for (ULevel* level : Levels)
	{
		if (!level)
		{
			continue;
		}
		for (AActor* Actor : level->GetActors())
		{
			QueueActorForBeginPlay(Actor);
		}
	}

	StartPendingObjects();
	PlayState = EWorldPlayState::Playing;
}

void UWorld::EndPlay()
{
	// 중복 종료와 재진입 차단
	if (bFlushingPendingDestroy || bClearingWorld || PlayState == EWorldPlayState::NotStarted 
		|| PlayState == EWorldPlayState::Ending)
	{
		return;
	}

	PlayState = EWorldPlayState::Ending;
	ReleaseExternalReferences();
	BeginPlayList.Reset();

	TArray<AActor*> EndingActors;

	// Level 외부 카메라 포함
	if (FallbackCamera)
	{
		EndingActors.Add(FallbackCamera);
	}

	for (ULevel* Level : Levels)
	{
		if (!Level)
		{
			continue;
		}

		for (AActor* Actor : Level->GetActors())
		{
			if (Actor)
			{
				EndingActors.Add(Actor);
			}
		}
	}

	// 전체 종료 후 초기화 해제
	for (AActor* Actor : EndingActors)
	{
		Actor->DispatchEndPlay();
	}

	for (AActor* Actor : EndingActors)
	{
		Actor->UninitializeComponents();
	}

	PlayState = EWorldPlayState::NotStarted;
}

UWorld* UWorld::CreateWorld(const EWorldType InWorldType, bool bInformEngineOfWorld, FName WorldName)
{
	UWorld* NewWorld = NewObject<UWorld>(nullptr, WorldName);
	NewWorld->SetFlags(EObjectFlags::RF_Transactional);
	NewWorld->WorldType = InWorldType;
	NewWorld->Init();

	return NewWorld;
}

UWorld* UWorld::GetDuplicatedWorldForPIE(UWorld * InWorld)
{
	if (!InWorld)
	{
		return nullptr;
	}

	UWorld* PIEWorld = CreateWorld(EWorldType::PIE, false);

	FObjectDuplicationParameters Params(InWorld, nullptr);
	Params.DuplicationSeed.Add(InWorld, PIEWorld);
	Params.DuplicationSeed.Add(InWorld->GetPersistentLevel(), PIEWorld->GetPersistentLevel());
	Params.PortFlags = EPropertyPortFlags::PPF_DuplicateForPIE;

	StaticDuplicateObjectEx(Params);
	return PIEWorld;
}

void UWorld::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// 아직 필요 없는애들
	//TArray<UObject*> ObjectsToFixReferences;
	//TMap<UObject*, UObject*> ReplacementMap;

	if (!bDuplicateForPIE)
	{
		assert(PersistentLevel);

		// Update the persistent level's owning world. This is needed for some initialization
		if (!PersistentLevel->OwningWorld)
		{
			PersistentLevel->OwningWorld = this;
		}
	}

	for (AActor* Actor : PersistentLevel->GetActors())
	{
		if (!Actor)
			continue;

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
				SceneComp->MarkTransformDirty();        // 부착/상대 Transform이 바뀌었으니 다시 계산
			Component->RegisterComponent();
		}

		QueueActorForBeginPlay(Actor);   // Tick 등록은 BeginPlay에서 함
	}

	Scene.BuildBVH();
}

void UWorld::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);   // "Properties"

	FArchive& Ar = Record.GetUnderlyingArchive();
	PersistentLevel->Serialize(Record.EnterRecord("PersistentLevel"));

	// ViewTarget 선택은 실행 상태이므로 여기서 별도 저장하지 않습니다.
}


void UWorld::QueueActorForBeginPlay(AActor* Actor)
{
	if (!Actor || Actor->GetWorld() != this || Actor->HasBegunPlay() || !Actor->CanRunGameplay())
	{
		return;
	}

	BeginPlayList.Enqueue(Actor->GetUUID());
}

void UWorld::QueueComponentForBeginPlay(UActorComponent* Component)
{
	if (!Component || Component->HasBegunPlay()	|| !Component->CanRunGameplay())
	{
		return;
	}

	if (Component->GetOwner()->GetWorld() != this)
	{
		return;
	}

	BeginPlayList.Enqueue(Component->GetUUID());
}

void UWorld::StartPendingObjects()
{
	if (!AcceptsChanges()||!IsRunningPlay())
	{
		return;
	}

	TArray<uint32> ActorsToStart;
	TArray<uint32> ComponentsToStart;

	// 콜백중 삭제를 대비해서 매번 탐색
	const auto FindComponent = [this](uint32 UUID) -> UActorComponent*
		{
			UActorComponent* Component = Cast<UActorComponent>(FindObjectByUUID(UUID));
			AActor* Owner = Component ? Component->GetOwner() : nullptr;
			return Owner && Owner->GetWorld() == this ? Component : nullptr;
		};

	
	const uint32 PendingCount = BeginPlayList.Num();
	for (uint32 Index = 0; Index < PendingCount; ++Index)
	{
		const uint32 UUID = BeginPlayList.Peek();
		BeginPlayList.Dequeue();

		if (AActor* Actor = Cast<AActor>(FindObjectByUUID(UUID)))
		{
			if (Actor->GetWorld() != this || Actor->HasBegunPlay())
			{
				continue;
			}

			ActorsToStart.Add(UUID);

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (Component)
				{
					ComponentsToStart.Add(Component->GetUUID());
				}
			}
		}
		else if (FindComponent(UUID))
		{
			ComponentsToStart.Add(UUID);
		}
	}

	for (uint32 UUID : ComponentsToStart)
	{
		if (UActorComponent* Component = FindComponent(UUID))
		{
			Component->DispatchInitializeComponent();
		}
	}

	for (uint32 UUID : ComponentsToStart)
	{
		if (UActorComponent* Component = FindComponent(UUID))
		{
			Component->DispatchBeginPlay();
		}
	}

	for (uint32 UUID : ActorsToStart)
	{
		AActor* Actor = Cast<AActor>(FindObjectByUUID(UUID));
		if (Actor && Actor->GetWorld() == this)
		{
			Actor->DispatchBeginPlay();
		}
	}

	// Fallback도 시작하지만, 선택되지 않았다면 Tick 등록은 해제합니다.
	if (FallbackCamera)
	{
		FallbackCamera->RegisterAllActorTickFunctions(MainViewTarget == FallbackCamera);
	}
}

void UWorld::Shutdown()
{
	// 중복 Shutdown과 정리 중 진입 차단
	if (bFlushingPendingDestroy || bShutdown || bShuttingDown || bClearingWorld || PlayState == EWorldPlayState::Ending)
	{
		return;
	}

	bShuttingDown = true;

	// 종료 World의 예약 폐기
	if (GEngine)
	{
		if (FWorldContext* Context = GEngine->GetWorldContextFromWorld(this))
		{
			Context->bSceneChangeRequested = false;
			Context->PendingScenePath.clear();
		}
	}

	EndPlay();
	ClearWorld();

	MainViewTarget = nullptr;

	// 내장 카메라 해제
	delete FallbackCamera;
	FallbackCamera = nullptr;

	// 소유 Level 해제
	for (ULevel* Level : Levels)
	{
		delete Level;
	}

	Levels.Reset();
	PersistentLevel = nullptr;
	CurrentLevel = nullptr;

	// 소유 객체 소멸 후 Lua VM 해제
	LuaRuntime.reset();

	bShutdown = true;
	bShuttingDown = false;
}

void UWorld::ReleaseExternalReferences()
{
	// 엔진의 비소유 참조 해제
	if (GEngine)
	{
		GEngine->ReleaseWorldReferences(this);
	}

	// World 수집 자료 정리
	VisibleProxies.Reset();
	LODInputs.Reset();
	SelectedLODs.Reset();

	for (FGatherChunk& Chunk : GatherChunks)
	{
		Chunk.Packets.Reset();
		Chunk.SlowPathIndices.Reset();

		for (FStaticDrawGroup& Group : Chunk.Groups)
		{
			Group.Items.clear();
		}
	}
}
