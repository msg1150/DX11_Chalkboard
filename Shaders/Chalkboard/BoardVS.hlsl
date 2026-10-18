// [칠판 프로젝트에서의 역할]
// 월드 좌표의 칠판 정점을 WVP로 화면에 투영하고 UV/색상을 Pixel Shader에 전달합니다.

cbuffer Scene : register(b0) { float4x4 WVP; uint UseInk; uint3 Padding; };
struct In {float3 position:POSITION;float3 normal:NORMAL;float2 uv:TEXCOORD;float4 color:COLOR;};
struct Out {float4 position:SV_POSITION;float2 uv:TEXCOORD;float4 color:COLOR;};
Out VSMain(In v)
{
    Out o;o.position=mul(float4(v.position,1),WVP);o.uv=v.uv;
    o.color=v.color;
    if(UseInk==0)o.color.rgb*=0.88+0.12*abs(v.normal.y);
    return o;
}
