#pragma once
#include "Render/PipelineState.h"
#include "Render/Buffer.h"
class FTexture2D;
struct FPostProcessSettings;

struct FGammaEncodeConstants
{
	// 첫 번째 16바이트: 독립적인 적용 여부와 선형 노출 배율이다.
	uint32 bEncode;
	uint32 bToneMap;
	float Exposure;
	uint32 bEnableVignette;

	// 두 번째 16바이트: Vignetting의 강도와 모양이다.
	float VignetteIntensity;
	float VignetteRadius;
	float VignetteSoftness;
	float Padding0;

	// 세 번째 16바이트: 픽셀 단위의 출력 크기와 정렬용 여백이다.
	float ViewWidth;
	float ViewHeight;
	float Padding1;
	float Padding2;
};

static_assert(sizeof(FGammaEncodeConstants) == 48);

// Linear 씬 컬러를 ACES로 톤매핑하고 화면 표시용 sRGB로 인코딩하는 후처리. 언리얼의 톤매퍼 패스에 해당한다.
class FGammaEncodeRenderer
{
public:
	FGammaEncodeRenderer() = default;
	~FGammaEncodeRenderer() = default;

	bool Init();
	// 현재 바인딩된 렌더 타깃에 SceneColor를 그린다. 렌더 패스는 호출하는 쪽이 연다.
	// ExposureEV는 톤매핑할 때만 쓴다.
	// bEncode는 출력의 sRGB 변환 가능 여부이며, Camera의 Gamma 스위치도 함께 확인한다.
	void OnRender(
		FTexture2D* SceneColor,
		bool bEncode,
		bool bToneMap,
		float ExposureEV,
		const FPostProcessSettings& PostProcessSettings);

	FGammaEncodeRenderer(const FGammaEncodeRenderer&) = delete;
	FGammaEncodeRenderer& operator=(const FGammaEncodeRenderer&) = delete;

private:
	FShaderProgram* Shader = nullptr;
	FPipelineState PipelineState;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
};
