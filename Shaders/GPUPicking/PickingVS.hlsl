// [칠판 프로젝트에서의 역할]
// Collider Proxy를 실제 카메라와 동일한 View/Projection으로 투영합니다.

// ============================================================================
// PickingVS.hlsl
// ----------------------------------------------------------------------------
// Collision Shape의 Solid Proxy를 Picking RenderTarget에 그리는 VS.
// Vertex는 이미 World Position이므로 ViewProjection만 적용한다.
// ============================================================================
cbuffer CBTransform : register(b0)
{
    float4x4 WorldViewProjection;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position=mul(float4(input.Position,1.0f),WorldViewProjection);
    return output;
}
