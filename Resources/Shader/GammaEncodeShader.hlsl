// 씬 컬러에 Tone Mapping과 Vignetting을 적용하고, Gamma ON일 때 sRGB로 변환한다.
// sRGB 변환은 출력 단계에서 한 번만 수행하며 각 효과는 독립적으로 켜고 끈다.
Texture2D SceneColor : register(t0);

cbuffer GammaEncodeConstants : register(b0)
{
    // 첫 번째 16바이트: 적용 여부와 노출 배율이다. C++ uint32는 HLSL uint에 대응한다.
    uint bEncode;
    uint bToneMap;
    float Exposure;
    uint bEnableVignette;

    // 두 번째 16바이트: Vignetting의 강도와 모양이다.
    float VignetteIntensity;
    float VignetteRadius;
    float VignetteSoftness;
    float Padding0;

    // 세 번째 16바이트: 픽셀 단위 출력 크기와 정렬용 여백이다.
    float ViewWidth;
    float ViewHeight;
    float Padding1;
    float Padding2;
};

struct PSInput
{
    float4 Position : SV_POSITION;
};

float3 ACESFitted(float3 color)
{
    const float3x3 ACESInputMat =
    {
        { 0.59719, 0.35458, 0.04823 },
        { 0.07600, 0.87577, 0.04823 },
        { 0.02840, 0.13383, 0.83777 }
    };
    const float3x3 ACESOutputMat =
    {
        { 1.60475, -0.53108, -0.07367 },
        { -0.10208, 1.10813, -0.00605 },
        { -0.00327, -0.07276, 1.07602 }
    };
    float3 v = mul(ACESInputMat, color);

    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    float3 c = a / b;
    return saturate(mul(ACESOutputMat, c));
}

// 정점 버퍼 없이 화면을 덮는 삼각형 하나를 만든다.
PSInput mainVS(uint VertexID : SV_VertexID)
{
    PSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2); // (0,0) (2,0) (0,2)
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

// sRGB 표준 전달 함수 (IEC 61966-2-1). Math/Color.h의 LinearToSRGB와 같은 식이다.
float3 LinearToSRGB(float3 L)
{
    float3 Low = L * 12.92f;
    float3 High = 1.055f * pow(L, 1.0f / 2.4f) - 0.055f;
    return lerp(High, Low, step(L, 0.0031308f));
}

float3 ApplyVignette(float3 Color, float2 PixelPosition)
{
    // 화면 중심을 원점으로 바꾼 뒤, 화면 높이의 절반을 거리 1로 사용한다.
    float2 ScreenCenter = float2(ViewWidth, ViewHeight) * 0.5f;
    float2 CenteredPosition = PixelPosition - ScreenCenter;
    
    // X와 Y를 같은 길이로 나눠 화면 비율이 달라도 원형을 유지한다.
    float2 NormalizedPosition = CenteredPosition / (ViewHeight * 0.5f);
    float DistanceFromCenter = length(NormalizedPosition);

    // Radius 안쪽은 0, Radius + Softness 바깥은 1이며 경계는 부드럽게 연결한다.
    float OuterRadius = VignetteRadius + VignetteSoftness;
    float VignetteMask = smoothstep(VignetteRadius, OuterRadius, DistanceFromCenter);

    // 중심부의 밝기는 유지하고, 최대 감쇠 영역은 (1 - Intensity)배 밝기로 만든다.
    float Darkness = VignetteIntensity * VignetteMask;
    
    return Color * (1.0f - Darkness);
}

float4 mainPS(PSInput Input) : SV_TARGET
{
    // 입력과 출력 크기가 같으므로 샘플러 없이 같은 픽셀을 그대로 읽는다.
    float3 Color = SceneColor.Load(int3(Input.Position.xy, 0)).rgb;

    // 각 효과를 별도의 조건으로 검사한다.
    if (bToneMap != 0)
    {
        Color = ACESFitted(Color * Exposure);
    }
    // 출력용 sRGB 변환 전에 감쇠를 적용한다.
    if (bEnableVignette != 0)
    {
        Color = ApplyVignette(Color, Input.Position.xy);
    }
    if (bEncode != 0)
    {
        Color = LinearToSRGB(Color);
    }

    // 뷰포트 텍스처를 ImGui가 알파 블렌딩으로 그리므로 최종 출력은 불투명으로 고정한다.
    return float4(Color, 1.0f);
}
