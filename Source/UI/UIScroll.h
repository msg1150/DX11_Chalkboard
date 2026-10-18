// [칠판 프로젝트에서의 역할]
// 스크롤 범위, Thumb 크기/위치, 휠 이동량을 계산하는 공용 수학 함수입니다.

// ============================================================================
// UIScroll.h
// ----------------------------------------------------------------------------
// 패널 Scrollbar의 계산만 담당하는 플랫폼/게임 독립 보조 모듈입니다.
// 실제 Mouse 입력 라우팅은 UIManager, 실제 그리기는 UIRenderer가 담당합니다.
//
// 이 파일을 분리한 이유:
// - Terrain/Camera를 전혀 알지 않습니다.
// - Scroll 범위/Thumb 크기/Drag 위치 계산을 독립 테스트할 수 있습니다.
// - Scroll 기능 제거 시 UIManager의 연결부와 이 파일만 제거할 수 있습니다.
// ============================================================================
#pragma once
#include "UI/UIRenderer.h"
#include <algorithm>

namespace UIScroll
{
    struct Geometry
    {
        UIRect Track{};
        UIRect Thumb{};
        float MaxOffset=0.f;
        bool Scrollable=false;
    };

    // 콘텐츠가 화면보다 클 때만 양수인 최대 스크롤량을 반환합니다.
    inline float MaxOffset(float contentHeight,float viewportHeight)
    {
        return std::max(0.f,contentHeight-viewportHeight);
    }

    inline float ClampOffset(float offset,float contentHeight,float viewportHeight)
    {
        return std::clamp(offset,0.f,MaxOffset(contentHeight,viewportHeight));
    }

    // Win32 Wheel Up은 +1, Wheel Down은 -1입니다.
    // 일반 UI 관례에 맞춰 Wheel Up이면 위로(Offset 감소), Down이면 아래로 이동합니다.
    // 휠 이동량을 픽셀 오프셋으로 환산하고 유효 스크롤 범위로 제한합니다.
    inline float ApplyWheel(float offset,float wheelDelta,float contentHeight,
                            float viewportHeight,float pixelsPerNotch)
    {
        return ClampOffset(offset-wheelDelta*pixelsPerNotch,
                           contentHeight,viewportHeight);
    }

    // 콘텐츠/뷰포트 비율로 Thumb 길이를, 현재 오프셋 비율로 Thumb 위치를 계산합니다.
    inline Geometry BuildGeometry(const UIRect& track,float contentHeight,
                                  float viewportHeight,float offset,
                                  float minimumThumbHeight)
    {
        Geometry result{};
        result.Track=track;
        result.MaxOffset=MaxOffset(contentHeight,viewportHeight);
        result.Scrollable=result.MaxOffset>0.f && track.H>0.f && track.W>0.f;
        if(!result.Scrollable)
        {
            result.Thumb=track;
            return result;
        }

        const float visibleRatio=std::clamp(viewportHeight/std::max(contentHeight,1.f),
                                             0.f,1.f);
        const float thumbHeight=std::min(track.H,
            std::max(minimumThumbHeight,track.H*visibleRatio));
        const float travel=std::max(0.f,track.H-thumbHeight);
        const float normalized=result.MaxOffset>0.f
            ? ClampOffset(offset,contentHeight,viewportHeight)/result.MaxOffset
            : 0.f;
        result.Thumb={track.X,track.Y+travel*normalized,track.W,thumbHeight};
        return result;
    }

    // Thumb의 위쪽 Y를 Scroll Offset으로 역변환합니다.
    // Drag 중 Mouse가 Track 밖으로 나가도 Clamp되어 안전하게 0~MaxOffset만 반환합니다.
    // 드래그된 Thumb 위치를 콘텐츠 스크롤 오프셋으로 역변환합니다.
    inline float OffsetFromThumbTop(float thumbTop,const Geometry& geometry)
    {
        if(!geometry.Scrollable || geometry.MaxOffset<=0.f)return 0.f;
        const float travel=std::max(0.f,geometry.Track.H-geometry.Thumb.H);
        if(travel<=0.f)return 0.f;
        const float clampedTop=std::clamp(thumbTop,
                                          geometry.Track.Y,
                                          geometry.Track.Y+travel);
        const float normalized=(clampedTop-geometry.Track.Y)/travel;
        return normalized*geometry.MaxOffset;
    }
}
