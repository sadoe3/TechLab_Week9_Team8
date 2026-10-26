#pragma once

#include "Engine.h"
#include "Engine/Viewport.h"
#include "Render/GammaEncodeRenderer.h"
#include "Render/SceneRenderer.h"
#include "Render/FogRenderer.h"
class UGameEngine : public UEngine
{
	DECLARE_CLASS(UGameEngine, UEngine)

public:
	FEngineConfig GetConfig() const override;

	// 엔진 생명주기
	virtual bool Init() override;
	virtual void PreTick(float DeltaTime) override;
	virtual void Tick(float DeltaTime) override;
	virtual void PostTick(float DeltaTime) override;
	virtual void PreExit() override;

	
	virtual void OnResize(uint32 Width, uint32 Height) override;
	virtual void ReleaseWorldReferences(UWorld* World) override;


private:
	// 게임 화면 렌더링
	void RenderGameView();

	// 실행 World의 비소유 참조
	UWorld* GameWorld = nullptr;

	// 게임 화면과 출력 자원 소유
	TUniquePtr<FViewport> GameViewport;
	TUniquePtr<FGammaEncodeRenderer> GammaEncodeRenderer;
	FSceneRenderer SceneRenderer;
	TUniquePtr<FFogRenderer> FogRenderer;
};