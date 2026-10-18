// [칠판 프로젝트에서의 역할]
// 위젯 높이를 이용해 1열/2열 위치와 콘텐츠 전체 높이를 계산합니다.
// 스크롤 오프셋과 Clip 사각형은 실제 그리기 및 입력 판정에 함께 사용합니다.

// ============================================================================
// UILayout.h
// ----------------------------------------------------------------------------
// Widget 타입/게임 기능을 모르는 공통 세로/두 열 배치 계산입니다.
// v0.3부터 Scroll Offset과 Scrollbar가 차지할 오른쪽 여백을 선택적으로 받습니다.
// 기본 인자값이 0이므로 기존 호출 코드는 그대로 사용할 수 있습니다.
// ============================================================================
#pragma once
#include "UI/UIRenderer.h"
#include "UI/UITheme.h"
#include <algorithm>
#include <cstddef>
#include <vector>

namespace UILayout
{
    struct Result
    {
        std::vector<UIRect> Items;
        UIRect ContentClip{};
        float RequiredContentHeight=0.f;
    };

    // 위젯 높이와 열 구분으로 각 사각형 및 전체 콘텐츠 높이를 계산하고 스크롤 오프셋을 적용합니다.
    inline Result Arrange(const UIRect& panel,const std::vector<float>& heights,
                          std::size_t rightColumnStart,float scrollOffsetY=0.f,
                          float rightInset=0.f)
    {
        Result result;
        const float left=panel.X+UITheme::PanelMargin;
        const float top=panel.Y+UITheme::PanelContentTop;
        const float viewportHeight=std::max(0.f,panel.H-UITheme::PanelContentTop-8.f);
        const float innerWidth=std::max(0.f,
            panel.W-UITheme::PanelMargin*2.f-std::max(0.f,rightInset));
        const bool twoColumns=rightColumnStart<heights.size();
        const float columnWidth=twoColumns
            ?std::max(0.f,(innerWidth-UITheme::ColumnGap)/2.f)
            :innerWidth;

        // 실제 Content 높이는 Scroll Offset과 무관해야 합니다.
        // 따라서 Column Y는 0부터 누적하고, 최종 화면 좌표를 만들 때만 Offset을 뺍니다.
        float leftContentY=0.f;
        float rightContentY=0.f;
        result.Items.reserve(heights.size());
        for(std::size_t i=0;i<heights.size();++i)
        {
            const bool right=twoColumns && i>=rightColumnStart;
            float& contentY=right?rightContentY:leftContentY;
            const float itemHeight=std::max(0.f,heights[i]);
            result.Items.push_back({
                left+(right?columnWidth+UITheme::ColumnGap:0.f),
                top+contentY-scrollOffsetY,
                columnWidth,
                itemHeight});
            contentY+=itemHeight+UITheme::ItemSpacing;
        }

        result.RequiredContentHeight=std::max(leftContentY,rightContentY);
        result.ContentClip={left,top,innerWidth,viewportHeight};
        return result;
    }

    inline bool Intersects(const UIRect& a,const UIRect& b)
    {
        return a.W>0.f && a.H>0.f && b.W>0.f && b.H>0.f &&
               a.X<b.X+b.W && b.X<a.X+a.W && a.Y<b.Y+b.H && b.Y<a.Y+a.H;
    }
}
