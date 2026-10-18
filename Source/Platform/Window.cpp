// [칠판 프로젝트에서의 역할]
// WindowProc에서 HWND와 C++ 객체를 연결하고 메시지를 Application에 전달합니다.
// 닫기 요청은 closeGuard_가 허용해야 처리하므로 저장하지 않은 그림을 확인할 수 있습니다.

// ============================================================================
// Window.cpp
// ============================================================================

#include "Platform/Window.h"

#include <utility>

Window::~Window()
{
    // Window가 아직 살아 있다면 OS 리소스를 정리한다.
    if (hwnd_)
    {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    // 등록했던 Window Class도 해제한다.
    if (hInstance_)
    {
        UnregisterClassW(
            className_.c_str(),
            hInstance_);
    }
}

bool Window::Initialize(
    HINSTANCE hInstance,
    UINT clientWidth,
    UINT clientHeight,
    const wchar_t* title)
{
    hInstance_ = hInstance;
    clientWidth_ = clientWidth;
    clientHeight_ = clientHeight;

    // ------------------------------------------------------------------------
    // Win32 Window Class 등록
    // ------------------------------------------------------------------------
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance_;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground =
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName =
        className_.c_str();

    if (!RegisterClassExW(&windowClass))
    {
        return false;
    }

    // 사용자가 요청한 값은 Client Area 크기다.
    // Window Frame / Title Bar를 포함한 실제 Window 크기로 변환한다.
    RECT windowRect =
    {
        0,
        0,
        static_cast<LONG>(clientWidth_),
        static_cast<LONG>(clientHeight_)
    };

    AdjustWindowRect(
        &windowRect,
        WS_OVERLAPPEDWINDOW,
        FALSE);

    // ------------------------------------------------------------------------
    // 실제 Win32 Window 생성
    //
    // 마지막 lpParam에 this를 전달하여
    // Static WindowProc에서 현재 Window 객체를 찾을 수 있게 한다.
    // ------------------------------------------------------------------------
    hwnd_ = CreateWindowExW(
        0,
        className_.c_str(),
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance_,
        this);

    if (!hwnd_)
    {
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    return true;
}

bool Window::ProcessMessages()
{
    MSG message = {};

    // PeekMessage를 사용하여 메시지가 없어도 Render Loop가 계속 진행되도록 한다.
    while (PeekMessageW(
        &message,
        nullptr,
        0,
        0,
        PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            return false;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return true;
}

void Window::SetMessageHandler(
    MessageHandler handler)
{
    messageHandler_ =
        std::move(handler);
}

HWND Window::GetHandle() const
{
    return hwnd_;
}

UINT Window::GetClientWidth() const
{
    RECT rect = {};
    GetClientRect(hwnd_, &rect);

    return static_cast<UINT>(
        rect.right -
        rect.left);
}

UINT Window::GetClientHeight() const
{
    RECT rect = {};
    GetClientRect(hwnd_, &rect);

    return static_cast<UINT>(
        rect.bottom -
        rect.top);
}

LRESULT CALLBACK Window::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    Window* window = nullptr;

    // WM_NCCREATE는 CreateWindowExW 직후 가장 먼저 들어오는 메시지 중 하나다.
    // 여기서 lpCreateParams에 넣었던 Window*를 꺼내 GWLP_USERDATA에 저장한다.
    if (message == WM_NCCREATE)
    {
        const auto createStruct =
            reinterpret_cast<CREATESTRUCTW*>(lParam);

        window =
            static_cast<Window*>(
                createStruct->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(window));
    }
    else
    {
        // 이후 메시지에서는 GWLP_USERDATA에서 기존 Window*를 다시 얻는다.
        window =
            reinterpret_cast<Window*>(
                GetWindowLongPtrW(
                    hwnd,
                    GWLP_USERDATA));
    }

    // Application이 등록한 Handler가 있다면
    // Input / Resize 처리용으로 메시지를 먼저 전달한다.
    if (window &&
        window->messageHandler_)
    {
        window->messageHandler_(
            hwnd,
            message,
            wParam,
            lParam);
    }

    switch (message)
    {
    case WM_CLOSE:
        if(window&&window->closeGuard_&&!window->closeGuard_())return 0;
        // 사용자가 X 버튼을 누르면 Window를 파괴한다.
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        // Window가 파괴되면 WM_QUIT을 발생시켜 메인 루프를 종료한다.
        PostQuitMessage(0);
        return 0;
    }

    // 직접 처리하지 않은 메시지는 Windows 기본 처리에 맡긴다.
    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}
