// [칠판 프로젝트에서의 역할]
// 패널의 표시 순서·마우스 캡처·숫자 편집 포커스·스크롤을 관리합니다.
// 위젯은 칠판 기능을 모르며 Getter/Setter와 버튼 콜백으로 값에 접근합니다.

// ============================================================================
// UIManager.h
// ----------------------------------------------------------------------------
// Terrain을 모르는 범용 다중 Panel + Widget + 입력 라우터입니다.
// v0.3부터 Panel Content가 Viewport보다 길면 자동 Scrollbar를 생성합니다.
// ============================================================================
#pragma once
#include "UI/UIWidget.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
class Input;
class UIManager
{
public:
    using PanelId=std::size_t;
    UIManager();

    // 기본 Panel(0) 호환 API: 이전 TerrainSettingsPanel 등록 코드를 유지합니다.
    void Add(std::unique_ptr<UIWidget> widget){AddToPanel(0,std::move(widget));}
    void StartSecondColumn(){StartSecondColumn(0);}
    bool IsPanelOpen()const{return IsPanelOpen(0);}

    PanelId CreatePanel(std::wstring title,std::wstring openLabel,
                        std::wstring closeLabel,float width,float height);
    void AddToPanel(PanelId id,std::unique_ptr<UIWidget> widget);
    void StartSecondColumn(PanelId id);
    bool IsPanelOpen(PanelId id)const;

    // Panel을 열지 않고 즉시 Callback을 실행하는 상단 Tool Button입니다.
    // UI Designer처럼 게임 화면 밖의 Tool을 여는 데 사용하며 Panel 시스템과 독립적입니다.
    std::size_t CreateActionButton(std::wstring label,std::function<void()> action);
    void Layout(float clientWidth,float clientHeight);

    // 반환값: Mouse Orbit 허용 여부.
    // Wheel은 Scroll 가능한 Panel 위에 있을 때 UI가 소비하고, 그 외에는 Camera Zoom으로 전달됩니다.
    bool Update(const Input& input,float clientWidth,float clientHeight);
    void Render(UIRenderer& renderer)const;

    bool IsMouseCaptured()const{return capturePanel_;}
    bool IsKeyboardCaptured()const{return focused_!=nullptr;}
    bool IsWheelCaptured()const{return wheelCapturedThisFrame_;}
    bool HitTest(float x,float y)const{return IsOverAnyUI(x,y);}
    void SetPanelOpen(PanelId id,bool open);

private:
    struct ActionButtonState
    {
        ActionButtonState(std::wstring label,std::function<void()> action)
            :button(std::move(label),std::move(action)){}
        UIButton button;
    };

    struct PanelState
    {
        PanelState(std::wstring name,std::wstring openText,std::wstring closeText,
                   float requestedWidth,float requestedHeight,
                   std::function<void()> onToggle)
            :title(std::move(name)),openLabel(std::move(openText)),
             closeLabel(std::move(closeText)),width(requestedWidth),height(requestedHeight),
             toggle(openLabel,std::move(onToggle)){}

        std::wstring title,openLabel,closeLabel;
        float width=0.f,height=0.f;
        UIPanel panel;
        UIButton toggle;
        std::vector<std::unique_ptr<UIWidget>> widgets;
        std::size_t secondColumnIndex=static_cast<std::size_t>(-1);
        UIRect contentClip{};
        bool open=false,manualPosition=false;
        float x=0.f,y=54.f;

        // Scroll 상태는 Panel별로 독립적입니다.
        float requiredContentHeight=0.f;
        float scrollOffsetY=0.f;
        bool scrollable=false;
    };

    void TogglePanel(PanelId id);
    void CommitFocusedEdit();
    bool IsOverAnyUI(float x,float y)const;
    UIRect ScrollTrackRect(const PanelState& panel)const;

    std::vector<std::unique_ptr<PanelState>> panels_;
    std::vector<std::unique_ptr<ActionButtonState>> actionButtons_;
    UIWidget* active_=nullptr;
    UIWidget* focused_=nullptr;
    PanelId captureId_=0;
    bool draggingHeader_=false;
    bool draggingScrollbar_=false;
    bool suppressOrbitUntilRelease_=false;
    bool capturePanel_=false;
    bool wheelCapturedThisFrame_=false;
    float grabOffsetX_=0.f,grabOffsetY_=0.f;
    float scrollThumbGrabOffsetY_=0.f;
    float mouseX_=0.f,mouseY_=0.f;
    float viewportWidth_=1280.f,viewportHeight_=720.f;
};
