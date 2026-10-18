// [칠판 프로젝트에서의 역할]
// 키보드 상태와 시간 순서가 보존된 마우스 이벤트 목록을 제공합니다.
// 한 프레임 안의 Down → 여러 Move → Up을 별도로 보존해야 짧은 클릭과 빠른 필기를 놓치지 않습니다.

#pragma once
#include <Windows.h>
#include <array>
#include <string>
#include <vector>
// Preserve ordered pointer events, including a full click within one frame.
class Input
{
public:
    enum class PointerKind {Down,Move,Up,Cancel};
    // 이벤트 당시 Client 좌표와 좌클릭 상태입니다. 현재 프레임의 마지막 상태와 구분합니다.
    struct PointerEvent {PointerKind kind;int x,y;bool left;};
    void BeginFrame();
    void ProcessMessage(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
    bool IsKeyDown(int key)const;
    bool IsKeyPressed(int key)const;
    bool IsKeyReleased(int key)const;
    bool IsLeftMouseDown()const{return left_;}
    bool IsLeftMousePressed()const{return pressed_;}
    bool IsLeftMouseReleased()const{return released_;}
    bool IsRightMouseDown()const{return right_;}
    int GetMouseDeltaX()const{return dx_;}
    int GetMouseDeltaY()const{return dy_;}
    float GetMouseWheelDelta()const{return wheel_;}
    int GetMouseX()const{return x_;}
    int GetMouseY()const{return y_;}
    const std::wstring& GetTextCharacters()const{return text_;}
    bool DidLoseFocus()const{return lost_;}
    const std::vector<PointerEvent>& PointerEvents()const{return events_;}
    Input PointerSample(const PointerEvent& event)const;
    Input NonPointerSample()const;
private:
    // 현재/이전 상태를 비교하여 키 Down과 이번 프레임 Pressed/Released를 구분합니다.
    std::array<bool,256> keys_{},previous_{};
    bool left_=false,right_=false,pressed_=false,released_=false,lost_=false,hasPosition_=false;
    int x_=0,y_=0,dx_=0,dy_=0;
    float wheel_=0;
    std::wstring text_;
    // 받은 순서를 유지합니다. Application이 UI와 필기에 이벤트 하나씩 전달합니다.
    std::vector<PointerEvent> events_;
};
