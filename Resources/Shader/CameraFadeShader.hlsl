cbuffer CameraFadeConstants : register(b0)
{
    // RGB는 C++에서 sRGB로 변환한 표시용 색상, A는 페이드 강도이다.
    float4 FadeColorAndAmount;
};

float4 mainVS(uint VertexID : SV_VertexID) : SV_POSITION
{
    // Draw(3)의 세 점 (-1,-1), (3,-1), (-1,3)이 화면 전체를 덮는다.
    float2 ClipPosition;
    ClipPosition.x = (VertexID == 1) ? 3.0f : -1.0f;
    ClipPosition.y = (VertexID == 2) ? 3.0f : -1.0f;
    return float4(ClipPosition, 0.0f, 1.0f);
}

float4 mainPS() : SV_TARGET
{
    // 기존 화면과의 혼합은 GPU의 AlphaBlend 상태가 수행한다.
    return FadeColorAndAmount;
}
