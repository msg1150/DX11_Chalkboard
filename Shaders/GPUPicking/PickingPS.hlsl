// [칠판 프로젝트에서의 역할]
// 화면 색상 대신 정수 Object ID와 화면 깊이를 각각 별도 MRT에 기록합니다.

// ============================================================================
// PickingPS.hlsl
// ----------------------------------------------------------------------------
// MRT0: 32-bit unsigned Object ID
// MRT1: 화면 Depth(0~1). CPU에서 Mouse X/Y와 함께 World Position 복원에 사용.
// ============================================================================
cbuffer CBPickObject : register(b0)
{
    uint ObjectID;
    float3 Padding;
};

struct PSInput
{
    float4 Position : SV_POSITION;
};

struct PSOutput
{
    uint ObjectID : SV_Target0;
    float Depth   : SV_Target1;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;
    output.ObjectID=ObjectID;
    output.Depth=input.Position.z;
    return output;
}
