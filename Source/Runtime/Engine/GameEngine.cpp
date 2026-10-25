#include "EnginePCH.h"
#include "GameEngine.h"
#include "Launch/LaunchEngineLoop.h"
#include "Serialization/JsonArchive.h"
#include "Core/Window.h"
#include "Engine/GameViewportClient.h"
#include "Render/ColorWorkflow.h"
#include "Render/RenderCommand.h"
#include "Render/Swapchain.h"

FEngineConfig UGameEngine::GetConfig() const
{
	FEngineConfig Desc;
	// TODO : 게임 이름으로 바꿔야함
	Desc.Title = L"Hitori";
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

	// 게임 World와 Context 연결
	GameWorld = UWorld::CreateWorld(EWorldType::Game, false);
	CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(GameWorld);

	
	// 시작 씬 로드
	/*
	// TODO: 시작 씬을 게임의 시작씬으로 옮겨야함
	const FString StartupScenePath = "Assets/Scenes/LinearColorTest.scene";
	*/

	// 차량 카메라와 주행 Lua가 연결된 테스트 씬으로 시작합니다.
	const FString StartupScenePath = "Assets/Scenes/Racing.scene";
	if (!FJsonArchive::LoadWorld(GameWorld, StartupScenePath))
	{
		HTR_LOG(Error, "Failed to load startup scene: {}", StartupScenePath);
		PreExit();
		return false;
	}

	// 화면 출력 초기화
	GammaEncodeRenderer = MakeUnique<FGammaEncodeRenderer>();
	if (!GammaEncodeRenderer->Init())
	{
		PreExit();
		return false;
	}

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
	GameWorld->Tick(DeltaTime);
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
	GammaEncodeRenderer.reset();

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

	// World 변경 전 렌더 참조 해제
	if (GameViewport)
	{
		GameViewport->ClearSceneData();
	}
	SceneRenderer.EndFrame();
	GetEngineLoop().GetRenderer()->ReleaseSceneReferences();
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
	SceneRenderer.PrepareView(*GameWorld, View, Data);
	FRenderer* Renderer = GetEngineLoop().GetRenderer();
	const FRenderingInfo& SceneInfo = GameViewport->GetSceneRenderingInfo();

	// 조명 설정
	FLightingConstants Lighting{};
	GameWorld->GetScene().BuildLightingConstants(Lighting);
	GameWorld->GetScene().BuildStructuredLight(Data.PointLights, Data.SpotLights, Data.CullingLights, View.ViewMatrix);
	Renderer->BindWorldShadows(InvalidShadowResourceIndex, SceneRenderer.GetFrameNumber(), false);
	Renderer->BindDirectionalShadow(*GameViewport, SceneRenderer.GetFrameNumber(), false);
	Renderer->SetLighting(Lighting, Data.PointLights, Data.SpotLights);
	Renderer->SetViewMode(View.RenderMode);
	Renderer->RenderQueueSorting(Data.RenderQueue, View.ViewProjection, View.GetCameraLocation());

	// 깊이 선행 패스
	FRenderingInfo DepthInfo = SceneInfo;
	DepthInfo.ColorRenderTargets.Reset();
	RenderCommand::BeginRenderPass(DepthInfo);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer->RenderDepthPrepass(View.ViewProjection);
	RenderCommand::EndRenderPass(DepthInfo);

	// 타일별 조명 계산
	Renderer->CullLights(
		GameViewport->GetLightCullingResources(),
		View.ProjectionMatrix,
		DepthInfo.DepthStencil.Texture,
		Data.CullingLights,
		DepthInfo.ViewportSetting.Width,
		DepthInfo.ViewportSetting.Height,
		false);

	// 불투명 및 반투명 패스
	FRenderingInfo ColorInfo = SceneInfo;
	ColorInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Load;
	const float ClearValue = ColorWorkflow::bLinear ? FLinearColor::SRGBToLinear(0.3f) : 0.3f;
	ColorInfo.ColorRenderTargets[0].ClearValue.ColorClearValue =
		FVector4(ClearValue, ClearValue, ClearValue, 1.0f);

	RenderCommand::BeginRenderPass(ColorInfo);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer->RenderOpaque(View.ViewProjection, true);
	Renderer->RenderTranslucent(View.ViewProjection);
	RenderCommand::EndRenderPass(ColorInfo);
	RenderCommand::UnbindShaderResources(8, 4, EShaderBindFlagBits::Pixel);
	RenderCommand::UnbindShaderResources(8, 2, EShaderBindFlagBits::Vertex);

	// 표시용 색 변환
	FRenderingInfo OutputInfo = GameViewport->GetRenderingInfo();
	OutputInfo.DepthStencil.Texture = nullptr;
	OutputInfo.ColorRenderTargets[0].LoadOp = ERenderTargetLoadOp::DontCare;
	RenderCommand::BeginRenderPass(OutputInfo);
	const bool bEncode = ColorWorkflow::bLinear;
	const bool bToneMap = bEncode && View.RenderMode.ViewMode == EViewMode::Lit;
	GammaEncodeRenderer->OnRender(SceneInfo.ColorRenderTargets[0].Texture, bEncode, bToneMap, 0.0f);
	RenderCommand::EndRenderPass(OutputInfo);
	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);

	// 백버퍼 복사 및 프레임 참조 해제
	const FRenderingInfo& BackbufferInfo = GetEngineLoop().GetSwapchain()->GetRenderingInfo();
	RenderCommand::CopyTexture(BackbufferInfo.ColorRenderTargets[0].Texture, GameViewport->GetColorTarget());
	Data.ReleaseSceneReferences();
	SceneRenderer.EndFrame();
}