// [칠판 프로젝트에서의 역할]
// 마우스 X 좌표 ↔ 슬라이더 값/비율을 변환하고 step 단위로 맞춥니다.

// ============================================================================
// UISliderMath.h : Slider의 마우스 위치 <-> 값 환산 (플랫폼 독립).
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>
namespace UISliderMath
{
    // 트랙에서의 비율을 값 범위로 바꾸고 step 단위로 반올림한 후 상한/하한을 적용합니다.
    inline float ValueFromX(float x,float left,float width,
                            float minimum,float maximum,float step) noexcept
    {
        if(width<=0.f || maximum<=minimum || step<=0.f)return minimum;
        const float t=std::clamp((x-left)/width,0.f,1.f);
        const float raw=minimum+t*(maximum-minimum);
        const float quantized=minimum+std::round((raw-minimum)/step)*step;
        return std::clamp(quantized,minimum,maximum);
    }
    // 현재 값을 0~1 비율로 바꿔 손잡이 위치를 정합니다. 값 범위가 0인 경우도 처리합니다.
    inline float Fraction(float value,float minimum,float maximum) noexcept
    {
        if(maximum<=minimum)return 0.f;
        return std::clamp((value-minimum)/(maximum-minimum),0.f,1.f);
    }
}
