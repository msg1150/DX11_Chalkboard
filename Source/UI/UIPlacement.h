// [칠판 프로젝트에서의 역할]
// 창 크기에 따라 패널/상단 버튼을 배치하고 화면 밖으로 벗어나지 않게 제한합니다.

// ============================================================================
// UIPlacement.h : 창 크기에 따른 UI 배치/이동 제한 (플랫폼 독립).
// 다른 UI 패널에서도 동일한 위치 제한 로직을 사용할 수 있다.
// ============================================================================
#pragma once
#include <algorithm>
#include <cstddef>
namespace UIPlacement
{
    struct Rect
    {
        float X=0.f,Y=0.f,W=0.f,H=0.f;
        bool Contains(float x,float y)const noexcept
        {return x>=X && y>=Y && x<X+W && y<Y+H;}
    };

    inline Rect SideButton(float clientWidth)
    {
        const float width=std::min(112.f,std::max(0.f,clientWidth-16.f));
        return {std::max(0.f,clientWidth-width-12.f),12.f,width,34.f};
    }

    // 여러 패널의 개별 열기/닫기 버튼을 우측에서 왼쪽으로 배치한다.
    // SideButton() 단일 버튼의 기존 위치는 그대로 유지한다.
    inline Rect SideButtonIndexed(float clientWidth,std::size_t index,std::size_t count)
    {
        if(count==0 || index>=count)return {};
        const float available=std::max(0.f,clientWidth-16.f);
        const float gap=8.f;
        const float width=std::max(0.f,std::min(112.f,
            (available-gap*static_cast<float>(count-1))/static_cast<float>(count)));
        const float right=std::max(0.f,clientWidth-12.f);
        return {std::max(0.f,right-width-static_cast<float>(index)*(width+gap)),
                12.f,width,34.f};
    }

    inline Rect ClampPanel(float desiredX,float desiredY,float panelWidth,
                           float panelHeight,float clientWidth,float clientHeight)
    {
        const float xMax=std::max(0.f,clientWidth-panelWidth-8.f);
        const float yMax=std::max(0.f,clientHeight-panelHeight-8.f);
        return {std::clamp(desiredX,0.f,xMax),
                std::clamp(desiredY,0.f,yMax),panelWidth,panelHeight};
    }
}
