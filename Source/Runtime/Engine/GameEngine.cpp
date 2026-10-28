#include "EnginePCH.h"
#include "GameEngine.h"
#include "Launch/LaunchEngineLoop.h"
#include "Serialization/JsonArchive.h"
#include "Core/Window.h"
#include "Core/TimeDilation.h"
#include "Engine/GameViewportClient.h"
#include "Render/RenderCommand.h"
#include "Render/Swapchain.h"
#include "Render/ViewMode.h"
#include "Component/TextRenderComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"

FEngineConfig UGameEngine::GetConfig() const
{
	FEngineConfig Desc;
	// TODO : 게임 이름으로 바꿔야함
	Desc.Title = L"Jungle Rider";
	Desc.Width = 1280;
	Desc.Height = 720;
	Desc.bBorderless = false;
	Desc.SyncInterval = 0;   // 0 = VSync 끔 (FPS 측정용)
	Desc.bCreateDepthBuffer = false;
	Desc.bExitOnEscape = false;        
	return Desc;
}

bool UGameEngine::Init()
{
	if (!Super::Init())
	{
		return false;
	}
	// 공통 장면 GPU 패스 초기화
	if (!GetEngineLoop().GetRenderer()->InitScenePasses())
	{
		return false;
	}
	// 게임 World와 Context 연결
	GameWorld = UWorld::CreateWorld(EWorldType::Game, false);
	CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(GameWorld);

	
	// 시작 씬 로드
	/*
	// TODO: 시작 씬을 게임의 시작씬으로 옮겨야함
	const FString StartupScenePath = "Assets/Scenes/LinearColorTest.scene";
	*/

	// 메인메뉴 작성용 씬으로 시작.
	const FString StartupScenePath = "Assets/Scenes/MainMenu.scene";
	if (!FJsonArchive::LoadWorld(GameWorld, StartupScenePath))
	{
		HTR_LOG(Error, "Failed to load startup scene: {}", StartupScenePath);
		PreExit();
		return false;
	}

	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();

	GameViewport = MakeUnique<FViewport>();
	GameViewport->SetOutputSize(
		GetEngineLoop().GetViewportWidth(),
		GetEngineLoop().GetViewportHeight());
	GameViewport->BeginPlay(MakeUnique<FGameViewportClient>(*GameWorld));

	// 게임 생명주기 시작
	GameWorld->BeginPlay();
	return true;
}

void UGameEngine::PreTick(float DeltaTime)
{
	// 프레임 시작 씬 전환
	FString ScenePath;
	if (ConsumeSceneChangeRequest(GameWorld, ScenePath))
	{
		ApplySceneChange(GameWorld, ScenePath);
	}

	// 창 포커스에 따른 카메라 입력
	FViewportInput Input;
	Input.bEnabled = ::GetForegroundWindow() == GetEngineLoop().GetMainWindow()->GetHandle();
	GameViewport->UpdateInput(Input);
}

void UGameEngine::Tick(float DeltaTime)
{
	// 게임 World 갱신
	GameWorld->Tick(DeltaTime * FTimeDilation::GetDilation());
	// 갱신 후 랜더 Transform 반영
	GameWorld->GetScene().UpdateAllTransforms();
	RenderGameView();
}

void UGameEngine::PostTick(float DeltaTime)
{
	// 프레임 종료 삭제 처리
	GameWorld->FlushPendingDestruction(GameWorld->GetPendingDestroyCount());
}

void UGameEngine::PreExit()
{
	// World 생존 중 Viewport 해제
	GameViewport.reset();
	TextRenderer.reset();

	// World 종료와 Context 해제
	if (GameWorld)
	{
		GameWorld->Shutdown();
		DestroyWorldContext(GameWorld);
		GameWorld = nullptr;
	}

	Super::PreExit();
}

void UGameEngine::OnResize(uint32 Width, uint32 Height)
{
	// 출력 크기 변경
	GameViewport->SetOutputSize(Width, Height);
}

void UGameEngine::ReleaseWorldReferences(UWorld* World)
{
	if (World != GameWorld)
	{
		return;
	}

	// World 종료·씬 정리 여부
	const bool bWorldEnding = !World->AcceptsChanges();

	// Viewport의 비소유 참조 정리
	if (GameViewport)
	{
		GameViewport->ClearSceneData();

		if (bWorldEnding)
		{
			// 이전 방향광 프레임 무효화
			GameViewport->GetDirectionalShadow().ResetFrame();
		}
	}

	// CPU·Renderer의 장면 참조 정리
	SceneRenderer.EndFrame();

	FRenderer* Renderer = GetEngineLoop().GetRenderer();
	Renderer->ReleaseSceneReferences();

	// World 정리 중 그림자 슬롯 반환
	if (bWorldEnding)
	{
		if (FWorldContext* Context = GetWorldContextFromWorld(World))
		{
			SceneRenderer.ReleaseWorldShadows(Context->ShadowResourceIndex);
			Renderer->ReleaseShadowResources(Context->ShadowResourceIndex);
			Context->ShadowResourceIndex = InvalidShadowResourceIndex;
		}
	}
}
void UGameEngine::RenderGameView()
{
	if (!GameViewport->IsVisible() || !GameViewport->GetActiveClient()->HasCamera())
	{
		GameViewport->ClearSceneData();
		return;
	}

	// View와 렌더 큐 준비
	GameViewport->EnsureRenderTargets();
	SceneRenderer.BeginFrame();

	const FSceneView View = GameViewport->CreateSceneView();
	FSceneRenderData& Data = GameViewport->GetSceneRenderData();
	FRenderer* Renderer = GetEngineLoop().GetRenderer();
	const bool bSupportsShadows = FSceneRenderer::SupportsShadows(View, RenderSettings);

	if (bSupportsShadows)
	{
		// 그림자 컬링용 View 등록
		SceneRenderer.AddShadowView(*GameWorld, View);
	}

	SceneRenderer.PrepareView(*GameWorld, View, Data);

	// Engine 세션의 그림자 슬롯 할당
	uint32 ShadowResourceIndex = InvalidShadowResourceIndex;
	if (bSupportsShadows)
	{
		if (FWorldContext* Context = GetWorldContextFromWorld(GameWorld))
		{
			if (Context->ShadowResourceIndex == InvalidShadowResourceIndex)
			{
				Context->ShadowResourceIndex = Renderer->AllocateShadowResources(false);
			}

			ShadowResourceIndex = Context->ShadowResourceIndex;
		}
	}

	// 공통 장면 패스 실행
	FShadowFrameData DirectionalFrame;
	SceneRenderer.RenderView(*Renderer, *GameViewport, *GameWorld, View, ShadowResourceIndex, RenderSettings, DirectionalFrame);

	// 표시용 컬러 위 텍스트 합성
	FRenderingInfo TextInfo = GameViewport->GetRenderingInfo();
	TextInfo.ColorRenderTargets[0].LoadOp = ERenderTargetLoadOp::Load;
	TextInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Load;

	RenderCommand::BeginRenderPass(TextInfo);
	RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);

	// 일반 텍스트 이후 최상단 텍스트 출력
	for (int32 Layer = 0; Layer < 2; ++Layer)
	{
		const bool bDrawOnTop = Layer == 1;
		for (TObjectIterator<UTextRenderComponent> Text; Text; ++Text)
		{
			// 현재 게임 World의 출력 대상 선별
			if (!Text->IsRegistered() || Text->IsPendingDestroy()
				|| !Text->IsVisible() || Text->IsHiddenInGame()
				|| !Text->GetFont() || Text->IsDrawOnTop() != bDrawOnTop)
			{
				continue;
			}

			const AActor* Owner = Text->GetOwner();
			if (!Owner || Owner->IsPendingDestroy() || Owner->GetWorld() != GameWorld)
			{
				continue;
			}

			TextRenderer->OnRender(
				Text->GetText(), Text->GetWorldMatrix(), Text->GetTextSize(),
				*Text->GetFont(), View.ViewProjection, bDrawOnTop);
		}
	}

	RenderCommand::EndRenderPass(TextInfo);
	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);

	// 백버퍼 복사·프레임 참조 해제
	const FRenderingInfo& BackbufferInfo = GetEngineLoop().GetSwapchain()->GetRenderingInfo();

	RenderCommand::CopyTexture(BackbufferInfo.ColorRenderTargets[0].Texture, GameViewport->GetColorTarget());

	Data.ReleaseSceneReferences();
	SceneRenderer.EndFrame();
}