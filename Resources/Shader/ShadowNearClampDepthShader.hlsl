#pragma pack_matrix(row_major)
// FShadowDepthPassConstants의 80-byte 배치를 공유한다.
cbuffer ShadowDepthPassConstants : register(b0)
{
    matrix ViewProjection;
    float3 LightPosition;
    float InvRadius;
};
cbuffer PerObjectConstants : register(b2)
{
    matrix World;
    matrix NormalMatrix;
};
struct VSOutput
{
    float4 Position : SV_POSITION;
    noperspective float Depth : TEXCOORD0;
    float FarClipDistance : SV_ClipDistance0;
};
VSOutput mainVS(float3 Position : POSITION)
{
    float4 WorldPosition = mul(float4(Position, 1.0f), World);
    float4 ClipPosition = mul(WorldPosition, ViewProjection);
    VSOutput Output;
    // 정점의 z는 유지한다. near를 가로지르는 삼각형도 원래 평면 깊이로 보간한다.
    Output.Position = ClipPosition;
    Output.Depth = ClipPosition.z / ClipPosition.w;
    // CSM 전용 rasterizer는 z clipping을 끄므로 far 평면은 명시적으로 유지한다.
    Output.FarClipDistance = ClipPosition.w - ClipPosition.z;
    return Output;
}
float mainPS(VSOutput Input) : SV_DEPTH
{
    // 직교 CSM의 원래 깊이를 보간한 뒤 near 앞의 픽셀만 붙인다.
    return Input.Depth < 0.0f ? 1.0e-6f : Input.Depth;
}
