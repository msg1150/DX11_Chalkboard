// [칠판 프로젝트에서의 역할]
// Win32 창과 메시지 펌프를 제공합니다. 입력/Resize 처리는 외부 콜백으로 전달합니다.

// ============================================================================
// Window.h
// ----------------------------------------------------------------------------
// Win32 Window 생성과 Message Pump를 담당하는 플랫폼 계층.
//
// Graphics나 Input이 직접 WndProc를 소유하지 않도록 분리한다.
// Window는 받은 메시지를 등록된 MessageHandler로 외부에 전달한다.
// ============================================================================

#pragma once

#include <Windows.h>

#include <functional>
#include <string>

class Window
{
public:
    // Application이 Win32 Message를 전달받기 위한 Callback 타입.
    using MessageHandler =
        std::function<void(HWND, UINT, WPARAM, LPARAM)>;

public:
    Window() = default;
    ~Window();

    // 지정한 Client Area 크기로 Win32 Window를 생성한다.
    bool Initialize(
        HINSTANCE hInstance,
        UINT clientWidth,
        UINT clientHeight,
        const wchar_t* title);

    // 현재 Message Queue에 쌓인 Win32 메시지를 모두 처리한다.
    // WM_QUIT을 만나면 false를 반환한다.
    bool ProcessMessages();

    // Application 쪽에서 메시지를 받을 Handler를 등록한다.
    void SetMessageHandler(MessageHandler handler);
    void SetCloseGuard(std::function<bool()> guard){closeGuard_=std::move(guard);}

    HWND GetHandle() const;

    // Window Frame을 제외한 실제 Client 영역 크기.
    UINT GetClientWidth() const;
    UINT GetClientHeight() const;

private:
    // Win32가 직접 호출하는 정적 Window Procedure.
    //
    // CreateWindowExW의 lpParam으로 전달한 Window*를
    // GWLP_USERDATA에 보관한 뒤 실제 객체로 메시지를 전달한다.
    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

private:
    HWND hwnd_ = nullptr;
    HINSTANCE hInstance_ = nullptr;

    UINT clientWidth_ = 0;
    UINT clientHeight_ = 0;

    std::wstring className_ =
        L"DXFrameworkWindowClass";

    MessageHandler messageHandler_;
    std::function<bool()> closeGuard_;
};
