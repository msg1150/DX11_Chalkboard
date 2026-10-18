// [칠판 프로젝트에서의 역할]
// 숫자 입력 문자열을 유효한 유한 값으로 파싱하고 허용 범위로 제한합니다.

// ============================================================================
// UINumericValue.h : 플랫폼 의존성 없는 숫자 입력 유효성 검사.
// 슬라이더 마우스 입력의 step과 달리 직접 입력한 유효한 숫자는 정밀도 유지.
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>
#include <string>
namespace UINumericValue
{
    // 문자열을 끝까지 검사해 부호/소수점을 처리합니다. 지수 표현이나 문자가 섞인 숫자는 허용하지 않습니다.
    inline bool ParseClamped(const std::wstring& text, float minimum,
                             float maximum, float& outValue) noexcept
    {
        if(text.empty() || text.size()>16 || !std::isfinite(minimum) ||
           !std::isfinite(maximum) || maximum<minimum) return false;

        std::size_t i=0;
        bool negative=false;
        if(text[i]==L'-' || text[i]==L'+')
        {
            negative=text[i]==L'-';
            ++i;
        }

        bool hasDigit=false,hasDot=false;
        double value=0.0,decimalScale=1.0;
        for(;i<text.size();++i)
        {
            const wchar_t ch=text[i];
            if(ch==L'.' && !hasDot){hasDot=true;continue;}
            if(ch<L'0'||ch>L'9')return false;
            hasDigit=true;
            const int digit=ch-L'0';
            if(!hasDot)value=value*10.0+digit;
            else {decimalScale*=0.1;value+=digit*decimalScale;}
        }
        if(!hasDigit || !std::isfinite(value))return false;
        if(negative)value=-value;
        outValue=static_cast<float>(std::clamp(value,
            static_cast<double>(minimum),static_cast<double>(maximum)));
        return true;
    }
}
