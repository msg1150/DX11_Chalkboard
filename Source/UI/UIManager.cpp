// [칠판 프로젝트에서의 역할]
// 앞에 보이는 UI부터 입력을 검사하고 드래그 소유권을 버튼을 놓을 때까지 유지합니다.
// Update가 false이면 Application은 해당 이벤트를 필기나 카메라 조작에 사용하지 않습니다.

// ============================================================================
// UIManager.cpp
// ----------------------------------------------------------------------------
// 다중 Panel의 Z-order, 입력 점유, Text Focus, Panel Scroll을 한 곳에서 관리합니다.
// Layout은 UILayout, Scroll 계산은 UIScroll, 화면 제한은 UIPlacement가 담당합니다.
// ============================================================================
#include "UI/UIManager.h"
#include "UI/UILayout.h"
#include "UI/UIPlacement.h"
#include "UI/UIScroll.h"
#include "UI/UITheme.h"
#include "Input/Input.h"
#include <algorithm>
#include <utility>

UIManager::UIManager()
{
    CreatePanel(L"분필 / 칠판",L"도구 열기",L"도구 닫기",350.f,646.f);
}

// 원하는 열림 상태와 다를 때만 TogglePanel을 호출하여 버튼 문구와 편집 상태를 함께 갱신합니다.
void UIManager::SetPanelOpen(PanelId id,bool open)
{if(id<panels_.size()&&panels_[id]->open!=open)TogglePanel(id);}

// 패널별 위젯/스크롤/드래그 상태를 독립적으로 생성하고 고유 인덱스를 반환합니다.
UIManager::PanelId UIManager::CreatePanel(std::wstring title,std::wstring openLabel,
                                         std::wstring closeLabel,float width,float height)
{
    const PanelId id=panels_.size();
    panels_.push_back(std::make_unique<PanelState>(
        std::move(title),std::move(openLabel),std::move(closeLabel),width,height,
        [this,id]{TogglePanel(id);}));
    // vector 원소는 unique_ptr로 관리하여 PanelState/Widget 주소를 유지합니다.
    return id;
}

// 위젯 소유권을 지정 패널에 등록합니다. 실제 위치는 Layout에서 결정합니다.
void UIManager::AddToPanel(PanelId id,std::unique_ptr<UIWidget> widget)
{
    if(widget && id<panels_.size())panels_[id]->widgets.push_back(std::move(widget));
}

// 현재 위젯 수를 두 번째 열의 시작 인덱스로 기록합니다.
void UIManager::StartSecondColumn(PanelId id)
{
    if(id<panels_.size())panels_[id]->secondColumnIndex=panels_[id]->widgets.size();
}

bool UIManager::IsPanelOpen(PanelId id)const
{
    return id<panels_.size() && panels_[id]->open;
}

// 패널 열기와 별개로 콜백을 실행하는 상단 버튼을 등록합니다.
std::size_t UIManager::CreateActionButton(std::wstring label,std::function<void()> action)
{
    const std::size_t id=actionButtons_.size();
    actionButtons_.push_back(std::make_unique<ActionButtonState>(std::move(label),std::move(action)));
    return id;
}

// 숫자 편집 중인 위젯의 값을 적용하고 키보드 포커스를 해제합니다.
void UIManager::CommitFocusedEdit()
{
    if(!focused_)return;
    focused_->CommitTextEdit();
    focused_=nullptr;
}

// 열림 상태와 버튼 라벨을 맞추고 패널/스크롤바 드래그 상태를 정리합니다.
void UIManager::TogglePanel(PanelId id)
{
    if(id>=panels_.size())return;
    CommitFocusedEdit();
    auto& p=*panels_[id];
    p.open=!p.open;
    p.toggle.SetLabel(p.open?p.closeLabel:p.openLabel);
    draggingHeader_=false;
    draggingScrollbar_=false;
}

// 패널의 제목과 여백을 제외한 우측 스크롤바 트랙 영역을 계산합니다.
UIRect UIManager::ScrollTrackRect(const PanelState& p)const
{
    const UIRect& b=p.panel.Bounds();
    return {
        b.X+b.W-UITheme::PanelMargin-UITheme::ScrollBarWidth,
        b.Y+UITheme::PanelContentTop,
        UITheme::ScrollBarWidth,
        std::max(0.f,b.H-UITheme::PanelContentTop-8.f)
    };
}

// 먼저 콘텐츠 높이를 측정하고 스크롤 필요 여부를 결정한 뒤 스크롤바 여백/오프셋을 적용해 재배치합니다.
void UIManager::Layout(float clientWidth,float clientHeight)
{
    viewportWidth_=std::max(0.f,clientWidth);
    viewportHeight_=std::max(0.f,clientHeight);

    const std::size_t totalTopButtons=panels_.size()+actionButtons_.size();
    for(PanelId i=0;i<panels_.size();++i)
    {
        auto& p=*panels_[i];
        const auto tab=UIPlacement::SideButtonIndexed(viewportWidth_,i,totalTopButtons);
        p.toggle.SetBounds({tab.X,tab.Y,tab.W,tab.H});

        const float panelWidth=std::min(p.width,std::max(0.f,viewportWidth_-24.f));
        const float panelHeight=std::min(p.height,std::max(0.f,viewportHeight_-16.f));
        if(!p.manualPosition)
        {
            p.x=std::max(0.f,viewportWidth_-panelWidth-UITheme::PanelMargin-
                static_cast<float>(i)*512.f);
            p.y=54.f;
        }

        const auto adjusted=UIPlacement::ClampPanel(
            p.x,p.y,panelWidth,panelHeight,viewportWidth_,viewportHeight_);
        p.x=adjusted.X;
        p.y=adjusted.Y;
        p.panel.SetBounds({adjusted.X,adjusted.Y,adjusted.W,adjusted.H});

        std::vector<float> heights;
        heights.reserve(p.widgets.size());
        for(const auto& widget:p.widgets)heights.push_back(widget->PreferredHeight());

        // 1차 배치는 Content 전체 높이를 알아내기 위한 측정용입니다.
        // Scrollbar가 필요한지 판정한 뒤 2차 배치에서 실제 Offset/오른쪽 여백을 적용합니다.
        const UILayout::Result measured=UILayout::Arrange(
            p.panel.Bounds(),heights,p.secondColumnIndex);
        p.requiredContentHeight=measured.RequiredContentHeight;
        p.scrollable=p.requiredContentHeight>measured.ContentClip.H+0.5f;

        if(!p.scrollable)
            p.scrollOffsetY=0.f;
        else
            p.scrollOffsetY=UIScroll::ClampOffset(
                p.scrollOffsetY,p.requiredContentHeight,measured.ContentClip.H);

        const float rightInset=p.scrollable
            ?UITheme::ScrollBarWidth+UITheme::ScrollBarGap
            :0.f;
        const UILayout::Result result=UILayout::Arrange(
            p.panel.Bounds(),heights,p.secondColumnIndex,p.scrollOffsetY,rightInset);
        p.contentClip=result.ContentClip;
        for(std::size_t w=0;w<p.widgets.size();++w)
            p.widgets[w]->SetBounds(result.Items[w]);
    }
    for(std::size_t i=0;i<actionButtons_.size();++i)
    {
        const auto tab=UIPlacement::SideButtonIndexed(
            viewportWidth_,panels_.size()+i,totalTopButtons);
        actionButtons_[i]->button.SetBounds({tab.X,tab.Y,tab.W,tab.H});
    }
}

// 상단 버튼과 열린 패널 전체 영역을 검사합니다. 위젯이 없는 패널 빈 공간도 장면 입력을 막습니다.
bool UIManager::IsOverAnyUI(float x,float y)const
{
    for(const auto& a:actionButtons_)
        if(a->button.HitTest(x,y))return true;
    for(const auto& p:panels_)
        if(p->toggle.HitTest(x,y) || (p->open && p->panel.Bounds().Contains(x,y)))
            return true;
    return false;
}

// 휠 → 클릭/드래그 소유권 → 해제 → 키보드 편집 순으로 처리합니다.
// 포인터가 UI 밖으로 나가도 UI에서 시작한 눌림은 놓을 때까지 캡처 상태를 유지합니다.
bool UIManager::Update(const Input& input,float width,float height)
{
    Layout(width,height);
    mouseX_=static_cast<float>(input.GetMouseX());
    mouseY_=static_cast<float>(input.GetMouseY());
    wheelCapturedThisFrame_=false;

    if(input.DidLoseFocus())
    {
        if(focused_)focused_->CancelTextEdit();
        focused_=nullptr;
        active_=nullptr;
        capturePanel_=false;
        draggingHeader_=false;
        draggingScrollbar_=false;
        suppressOrbitUntilRelease_=false;
    }

    // ---------------------------------------------------------------------
    // Mouse Wheel Scroll
    // ---------------------------------------------------------------------
    // 가장 앞쪽의 Scroll 가능한 Panel Content 위에서만 Wheel을 UI가 소비합니다.
    // 이 경우 Application이 CameraController의 Zoom을 막고, Panel 밖에서는 기존 Zoom을 유지합니다.
    const float wheel=input.GetMouseWheelDelta();
    if(wheel!=0.f)
    {
        for(PanelId reverse=panels_.size();reverse>0;--reverse)
        {
            auto& p=*panels_[reverse-1];
            if(!p.open || !p.scrollable || !p.panel.Bounds().Contains(mouseX_,mouseY_))continue;

            const auto geometry=UIScroll::BuildGeometry(
                ScrollTrackRect(p),p.requiredContentHeight,p.contentClip.H,
                p.scrollOffsetY,UITheme::ScrollMinThumbHeight);
            if(p.contentClip.Contains(mouseX_,mouseY_) || geometry.Track.Contains(mouseX_,mouseY_))
            {
                p.scrollOffsetY=UIScroll::ApplyWheel(
                    p.scrollOffsetY,wheel,p.requiredContentHeight,p.contentClip.H,
                    UITheme::ScrollWheelStep);
                wheelCapturedThisFrame_=true;
                Layout(width,height);
                break;
            }
        }
    }

    if(input.IsLeftMousePressed() && !input.DidLoseFocus())
    {
        const bool hadKeyboardFocus=focused_!=nullptr;
        CommitFocusedEdit();
        if(hadKeyboardFocus && !IsOverAnyUI(mouseX_,mouseY_))
            suppressOrbitUntilRelease_=true;

        // Panel Toggle과 별도 Tool Action Button은 Panel보다 위에 그립니다.
        bool selected=false;
        for(std::size_t reverse=actionButtons_.size();reverse>0;--reverse)
        {
            auto& action=*actionButtons_[reverse-1];
            if(action.button.HitTest(mouseX_,mouseY_))
            {
                active_=&action.button;
                // Action Button은 Panel Drag가 아니므로 captureId_를 사용하지 않습니다.
                // Release까지 Orbit만 억제하고 UIButton::PointerUp에서 Callback을 실행합니다.
                capturePanel_=false;
                suppressOrbitUntilRelease_=true;
                draggingHeader_=false;
                draggingScrollbar_=false;
                selected=true;
                break;
            }
        }
        if(!selected)
        for(PanelId reverse=panels_.size();reverse>0;--reverse)
        {
            const PanelId id=reverse-1;
            if(panels_[id]->toggle.HitTest(mouseX_,mouseY_))
            {
                capturePanel_=true;
                captureId_=id;
                draggingHeader_=false;
                draggingScrollbar_=false;
                active_=&panels_[id]->toggle;
                selected=true;
                break;
            }
        }

        if(!selected)
        {
            // 뒤에 그리는 Panel일수록 전면입니다. 전면 Panel의 빈 공간 클릭은
            // 아래 Panel Widget으로 통과시키지 않습니다.
            for(PanelId reverse=panels_.size();reverse>0;--reverse)
            {
                const PanelId id=reverse-1;
                auto& p=*panels_[id];
                if(!p.open || !p.panel.Bounds().Contains(mouseX_,mouseY_))continue;

                capturePanel_=true;
                captureId_=id;
                active_=nullptr;
                draggingScrollbar_=false;

                if(mouseY_<p.panel.Bounds().Y+UITheme::PanelHeaderHeight)
                {
                    draggingHeader_=true;
                    grabOffsetX_=mouseX_-p.panel.Bounds().X;
                    grabOffsetY_=mouseY_-p.panel.Bounds().Y;
                }
                else
                {
                    draggingHeader_=false;

                    // Scrollbar는 Content Widget보다 먼저 HitTest합니다.
                    // Thumb Drag와 Track Click이 Slider 같은 Widget으로 전달되지 않게 하기 위함입니다.
                    if(p.scrollable)
                    {
                        const auto geometry=UIScroll::BuildGeometry(
                            ScrollTrackRect(p),p.requiredContentHeight,p.contentClip.H,
                            p.scrollOffsetY,UITheme::ScrollMinThumbHeight);
                        if(geometry.Track.Contains(mouseX_,mouseY_))
                        {
                            draggingScrollbar_=true;
                            if(geometry.Thumb.Contains(mouseX_,mouseY_))
                            {
                                scrollThumbGrabOffsetY_=mouseY_-geometry.Thumb.Y;
                            }
                            else
                            {
                                // Track의 빈 공간을 누르면 Thumb 중심을 Mouse 위치로 점프시킨 뒤
                                // 그대로 Drag를 이어갈 수 있게 합니다.
                                scrollThumbGrabOffsetY_=geometry.Thumb.H*0.5f;
                                p.scrollOffsetY=UIScroll::OffsetFromThumbTop(
                                    mouseY_-scrollThumbGrabOffsetY_,geometry);
                                Layout(width,height);
                            }
                            break;
                        }
                    }

                    if(p.contentClip.Contains(mouseX_,mouseY_))
                    {
                        for(auto it=p.widgets.rbegin();it!=p.widgets.rend();++it)
                        {
                            if(UILayout::Intersects((*it)->Bounds(),p.contentClip) &&
                               (*it)->HitTest(mouseX_,mouseY_))
                            {
                                active_=it->get();
                                active_->PointerDown(mouseX_,mouseY_);
                                if(active_->IsTextEditing())focused_=active_;
                                break;
                            }
                        }
                    }
                }
                break;
            }
        }
    }
    else if(input.IsLeftMouseDown() && capturePanel_)
    {
        if(draggingHeader_)
        {
            auto& p=*panels_[captureId_];
            p.manualPosition=true;
            p.x=mouseX_-grabOffsetX_;
            p.y=mouseY_-grabOffsetY_;
            Layout(width,height);
        }
        else if(draggingScrollbar_)
        {
            auto& p=*panels_[captureId_];
            const auto geometry=UIScroll::BuildGeometry(
                ScrollTrackRect(p),p.requiredContentHeight,p.contentClip.H,
                p.scrollOffsetY,UITheme::ScrollMinThumbHeight);
            p.scrollOffsetY=UIScroll::OffsetFromThumbTop(
                mouseY_-scrollThumbGrabOffsetY_,geometry);
            Layout(width,height);
        }
        else if(active_ && active_!=&panels_[captureId_]->toggle)
            active_->PointerDrag(mouseX_,mouseY_);
    }

    const bool wasCaptured=capturePanel_ || suppressOrbitUntilRelease_;
    if(input.IsLeftMouseReleased() ||
       (!input.IsLeftMouseDown() && (capturePanel_ || suppressOrbitUntilRelease_)))
    {
        if(active_ && input.IsLeftMouseReleased())active_->PointerUp(mouseX_,mouseY_);
        active_=nullptr;
        capturePanel_=false;
        draggingHeader_=false;
        draggingScrollbar_=false;
        suppressOrbitUntilRelease_=false;
    }

    if(focused_ && input.IsKeyPressed(VK_ESCAPE))
    {
        focused_->CancelTextEdit();
        focused_=nullptr;
    }
    if(focused_)
        for(wchar_t ch:input.GetTextCharacters())
        {
            focused_->HandleTextCharacter(ch);
            if(!focused_->IsTextEditing())
            {
                focused_=nullptr;
                break;
            }
        }

    return !(IsOverAnyUI(mouseX_,mouseY_) || wasCaptured || focused_!=nullptr);
}

// 패널 등록 순서대로 그리되 Hover는 가장 앞 패널만 적용합니다. 상단 버튼은 패널보다 나중에 그립니다.
void UIManager::Render(UIRenderer& renderer)const
{
    // Hover도 실제 Pointer 입력과 동일한 Z-order를 사용합니다.
    const PanelState* topHovered=nullptr;
    for(auto it=panels_.rbegin();it!=panels_.rend();++it)
        if((*it)->open && (*it)->panel.Bounds().Contains(mouseX_,mouseY_))
        {
            topHovered=it->get();
            break;
        }

    for(const auto& p:panels_)
    {
        if(!p->open)continue;
        p->panel.Render(renderer,p->title);
        const UIRect& b=p->panel.Bounds();

        // 안내 문구는 충분히 넓은 기본 패널에만 표시하여 칠판 도구 제목과 겹치지 않게 합니다.
        if(p.get()==panels_.front().get() && b.W>=500.f)
            renderer.Text(L"제목줄 드래그: 이동 / Wheel: 스크롤",
                          {b.X+b.W-250.f,b.Y+11.f,232.f,23.f},
                          UITheme::Muted,UIFont::Small);

        if(p->contentClip.W>0.f && p->contentClip.H>0.f)
        {
            renderer.PushClip(p->contentClip);
            for(const auto& widget:p->widgets)
                if(UILayout::Intersects(widget->Bounds(),p->contentClip))
                    widget->Render(renderer,
                        (p.get()==topHovered && p->contentClip.Contains(mouseX_,mouseY_) &&
                         widget->HitTest(mouseX_,mouseY_)) ||
                        (capturePanel_ && widget.get()==active_));
            renderer.PopClip();
        }

        if(p->scrollable)
        {
            const auto geometry=UIScroll::BuildGeometry(
                ScrollTrackRect(*p),p->requiredContentHeight,p->contentClip.H,
                p->scrollOffsetY,UITheme::ScrollMinThumbHeight);
            const bool hoverThumb=(p.get()==topHovered && geometry.Thumb.Contains(mouseX_,mouseY_)) ||
                                  (capturePanel_ && draggingScrollbar_ &&
                                   p.get()==panels_[captureId_].get());
            renderer.FillRoundRect(geometry.Track,UITheme::ScrollTrack,5.f);
            renderer.StrokeRect(geometry.Track,UITheme::ScrollBorder,1.f);
            renderer.FillRoundRect(geometry.Thumb,
                hoverThumb?UITheme::ScrollThumbHover:UITheme::ScrollThumb,5.f);
        }
    }

    // Panel보다 위에 출력하므로 열린 Panel이 Toggle Button을 가리지 않습니다.
    for(const auto& p:panels_)
        p->toggle.Render(renderer,p->toggle.HitTest(mouseX_,mouseY_));
    for(const auto& action:actionButtons_)
        action->button.Render(renderer,action->button.HitTest(mouseX_,mouseY_));
}
