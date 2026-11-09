cbuffer CameraLetterBoxConstants : register(b0)
{
    float HeightRatio; // 위·아래 각각의 띠 높이 비율: 0~0.5
    float3 Padding;    // 상수 버퍼 크기를 16바이트로 맞춘다.
};

struct FVertexOutput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

FVertexOutput mainVS(uint VertexID : SV_VertexID)
{
    // Vertex Buffer 없이 Draw(3)으로 화면을 덮는 삼각형을 만든다.
    float2 ClipPosition;
    ClipPosition.x = (VertexID == 1) ? 3.0f : -1.0f;
    ClipPosition.y = (VertexID == 2) ? 3.0f : -1.0f;

    FVertexOutput Output;
    Output.Position = float4(ClipPosition, 0.0f, 1.0f);
    // 화면 내부에서 위쪽 UV.y는 0, 아래쪽은 1이다.
    // 화면 밖 삼각형 꼭짓점의 UV는 0~1 범위를 벗어날 수 있다.
    Output.UV = ClipPosition * float2(0.5f, -0.5f) + 0.5f;
    return Output;
}

float4 mainPS(FVertexOutput Input) : SV_TARGET
{
    // 가운데 영역에는 기록하지 않는다. Render Pass의 Load로 기존 화면을 보존한다.
    if (Input.UV.y >= HeightRatio
        && Input.UV.y < 1.0f - HeightRatio)
    {
        discard;
    }

    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
