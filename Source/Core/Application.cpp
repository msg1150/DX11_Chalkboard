// [칠판 프로젝트에서의 역할]
// 한 프레임은 입력 수집 → UI/필기 처리 → 카메라 처리 → 3D/2D 렌더링 순서입니다.
// UI가 먼저 입력을 소비하며 필기 드래그 동안 카메라가 움직이지 않게 제어합니다.

#include "Core/Application.h"
#include <algorithm>
#include <filesystem>
using namespace DirectX;
// Win32/DX11/UI/카메라/칠판을 의존 순서대로 초기화하고 버튼 콜백을 등록합니다.
bool Application::Initialize(HINSTANCE instance)
{
    window_.SetMessageHandler([this](HWND h,UINT m,WPARAM w,LPARAM l){HandleMessage(h,m,w,l);});
    window_.SetCloseGuard([this]{return !ready_||board_.ConfirmDiscard();});
    if(!window_.Initialize(instance,1440,900,L"DX11 Chalkboard - 분필 칠판")){error_=L"Window initialization failed";return false;}
    if(!renderer_.Initialize(window_.GetHandle(),window_.GetClientWidth(),window_.GetClientHeight()))
    {error_=L"D3D11 initialization failed";return false;}
    if(!uiRenderer_.Initialize(renderer_.GetSwapChain())){error_=uiRenderer_.GetError();return false;}
    camera_.SetPerspective(XMConvertToRadians(45.f),float(window_.GetClientWidth())/float(window_.GetClientHeight()),.1f,400.f);
    ResetCamera();
    wchar_t executable[32768]{};const auto n=GetModuleFileNameW(nullptr,executable,32768);
    if(n==0||n>=32768){error_=L"Executable path unavailable";return false;}
    if(!board_.Initialize(renderer_.GetDevice(),std::filesystem::path(executable).parent_path(),window_.GetHandle()))
    {error_=board_.Error();return false;}
    board_.RegisterUI(ui_,[this]{ResetCamera();});
    ui_.Layout(float(window_.GetClientWidth()),float(window_.GetClientHeight()));
    ready_=true;return true;
}
// 칠판을 정면에서 보되 우측 도구 패널과 겹치지 않도록 시선 중심을 오른쪽으로 옮깁니다.
void Application::ResetCamera()
{camera_.SetTarget({2.5f,-.15f,0});camera_.SetOrbit(XM_PI,0,16.5f);}
// 입력 프레임 초기화 후 메시지를 모두 수집하여 Update/Render합니다. 최소화 중에는 메시지를 기다립니다.
int Application::Run()
{
    while(true)
    {
        input_.BeginFrame();if(!window_.ProcessMessages())break;
        if(!window_.GetClientWidth()||!window_.GetClientHeight()){WaitMessage();continue;}
        Update();Render();
    }
    return 0;
}
// 원본 Win32 메시지를 Input에 전달하고 포커스/크기/우클릭 시작 같은 창 상태 변화를 처리합니다.
void Application::HandleMessage(HWND hwnd,UINT message,WPARAM w,LPARAM l)
{
    input_.ProcessMessage(hwnd,message,w,l);
    if(message==WM_KILLFOCUS||message==WM_CANCELMODE)
    {board_.stroke.End(board_.canvas);rightDrag_=false;}
    if(message==WM_RBUTTONDOWN&&ready_)
        rightDrag_=!input_.IsLeftMouseDown()&&!ui_.HitTest(float(input_.GetMouseX()),float(input_.GetMouseY()));
    if(message==WM_RBUTTONUP)rightDrag_=false;
    if(message==WM_SIZE&&ready_)
    {
        board_.stroke.End(board_.canvas);
        const UINT width=LOWORD(l),height=HIWORD(l);
        if(width&&height)
        {
            uiRenderer_.PrepareForResize();renderer_.Resize(width,height);
            if(!uiRenderer_.RecreateTarget())error_=uiRenderer_.GetError();
            camera_.SetAspectRatio(float(width)/float(height));ui_.Layout(float(width),float(height));
        }
    }
}
// 수집한 마우스 이벤트마다 UI 소유권을 먼저 판정한 뒤 필기 좌표를 전달합니다.
// 필기가 있었던 프레임 전체에서 카메라를 막고 텍스트 입력 중에는 편집 단축키도 전달하지 않습니다.
void Application::Update()
{
    const auto width=window_.GetClientWidth(),height=window_.GetClientHeight();
    bool drawingThisFrame=board_.stroke.Active();
    // Copy because native file dialogs can re-enter the message handler.
    const auto events=input_.PointerEvents();
    for(const auto& event:events)
    {
        const auto sample=input_.PointerSample(event);
        // 이벤트 당시 좌표로 UI를 먼저 검사합니다. UI에서 시작한 드래그는 판에 옮겨도 available=false를 유지합니다.
        const bool available=ui_.Update(sample,float(width),float(height));
        if(event.kind==Input::PointerKind::Down)
        {
            board_.stroke.Begin(board_.canvas,available&&!input_.IsRightMouseDown());
            if(board_.stroke.Active())board_.RequestPick(event.x,event.y);
        }
        drawingThisFrame|=board_.stroke.Active();
        if(event.kind==Input::PointerKind::Cancel)
        {board_.stroke.End(board_.canvas);continue;}
        board_.stroke.Sample(board_.canvas,board_.Project(event.x,event.y,width,height,camera_),!available);
        if(event.kind==Input::PointerKind::Up)board_.stroke.End(board_.canvas);
    }
    const bool available=ui_.Update(input_.NonPointerSample(),float(width),float(height));
    if(input_.DidLoseFocus()||!input_.IsLeftMouseDown())board_.stroke.End(board_.canvas);
    // 판 밖에서도 드래그 소유권을 유지합니다. 같은 프레임에 MouseUp이 와도 이미 그렸다면 카메라를 움직이지 않습니다.
    const bool cameraAllowed=available&&!drawingThisFrame&&!input_.IsLeftMouseDown()&&!input_.DidLoseFocus();
    if(cameraAllowed)
    {
        if(rightDrag_&&input_.IsRightMouseDown())camera_.AddOrbit(float(input_.GetMouseDeltaX())*.006f,float(input_.GetMouseDeltaY())*.006f);
        if(!ui_.IsWheelCaptured())camera_.AddDistance(-input_.GetMouseWheelDelta());
        if(input_.IsKeyPressed(VK_HOME)&&!ui_.IsKeyboardCaptured())ResetCamera();
    }
    if(!drawingThisFrame&&!input_.IsLeftMouseDown()&&!ui_.IsKeyboardCaptured())
    {
        if(input_.IsKeyDown(VK_CONTROL))
        {
            if(input_.IsKeyPressed('S'))board_.SaveDialog();
            if(input_.IsKeyPressed('O'))board_.LoadDialog(input_.IsKeyDown(VK_SHIFT));
            if(input_.IsKeyPressed('Z'))board_.canvas.Undo();
            if(input_.IsKeyPressed('Y'))board_.canvas.Redo();
        }
    }
}
// 배경/깊이 초기화 → 칠판 3D → 안내와 도구 UI → Present의 순서입니다.
void Application::Render()
{
    constexpr float background[]={.60f,.83f,.88f,1};renderer_.BeginFrame(background);
    board_.Render(renderer_.GetContext(),renderer_.GetDepthStencilView(),camera_,renderer_.GetWidth(),renderer_.GetHeight());
    if(uiRenderer_.Begin(renderer_.GetContext()))
    {
        board_.RenderOverlay(uiRenderer_,float(window_.GetClientWidth()),float(window_.GetClientHeight()));
        ui_.Render(uiRenderer_);uiRenderer_.End();
    }
    renderer_.EndFrame();
}
