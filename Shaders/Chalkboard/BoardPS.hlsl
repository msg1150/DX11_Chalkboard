// [칠판 프로젝트에서의 역할]
// UseInk가 켜진 앞면은 그림 텍스처와 원래 초록색 바탕을 합성합니다.
// 지운 픽셀은 RGBA=0이므로 그림 기여도가 없어지고 원래 바탕이 보입니다.

cbuffer Scene : register(b0) {float4x4 WVP;uint UseInk;uint3 Padding;};
Texture2D Ink : register(t0);
SamplerState LinearClamp : register(s0);
struct In {float4 position:SV_POSITION;float2 uv:TEXCOORD;float4 color:COLOR;};
float4 PSMain(In v):SV_TARGET
{
    if(UseInk==0)return v.color;
    // CPU upload is premultiplied, so linear filtering has no dark edge halo.
    float4 ink=Ink.Sample(LinearClamp,v.uv);
    return float4(ink.rgb+v.color.rgb*(1-ink.a),1);
}
