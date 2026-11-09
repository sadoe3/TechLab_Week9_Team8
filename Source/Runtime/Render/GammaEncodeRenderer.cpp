#include "EnginePCH.h"

#include "GammaEncodeRenderer.h"
#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "Texture2D.h"
#include "Render/PostProcessSettings.h"
#include "Math/EngineMath.h"

bool FGammaEncodeRenderer::Init()
{
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/GammaEncodeShader.hlsl");
	if (!Shader)
	{
		HTR_LOG(Error, "[GammaEncode] shader not found");
		return false;
	}

	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FGammaEncodeConstants));

	PipelineState.Shader = Shader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	// 출력 타깃 전체를 덮어쓰는 후처리라 블렌딩·깊이 테스트가 필요 없다
	PipelineState.RasterizerState = ERasterizerState::SolidNone;
	PipelineState.BlendState = EBlendState::Opaque;
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;
	return true;
}

void FGammaEncodeRenderer::OnRender(
	FTexture2D* SceneColor,
	bool bEncode,
	bool bToneMap,
	float ExposureEV,
	const FPostProcessSettings& PostProcessSettings)
{
	if (!Shader || !SceneColor)
		return;

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindShaderResource(0, SceneColor, EShaderBindFlagBits::Pixel);

	// Padding을 포함한 모든 필드를 0으로 초기화한다.
	FGammaEncodeConstants Constants{};
	Constants.bEncode =
		(bEncode && PostProcessSettings.bEnableGammaCorrection) ? 1u : 0u;
	// Gamma OFF여도 Tone Mapping과 Vignetting은 독립적으로 적용한다.
	Constants.bToneMap = bToneMap ? 1u : 0u;
	Constants.bEnableVignette = PostProcessSettings.bEnableVignette ? 1u : 0u;
	Constants.Exposure = std::exp2(ExposureEV);

	// GPU의 감쇠 계산에 사용할 값을 유효 범위로 제한한다.
	Constants.VignetteIntensity =
		FMath::Clamp(PostProcessSettings.VignetteIntensity, 0.0f, 1.0f);
	Constants.VignetteRadius = FMath::Max(PostProcessSettings.VignetteRadius, 0.0f);
	Constants.VignetteSoftness =
		FMath::Max(PostProcessSettings.VignetteSoftness, 0.0001f);

	// 현재 Output Pass는 입력과 출력의 크기가 같다.
	Constants.ViewWidth = static_cast<float>(SceneColor->GetWidth());
	Constants.ViewHeight = static_cast<float>(SceneColor->GetHeight());
	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Constants);
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);

	RenderCommand::Draw(3);
}
