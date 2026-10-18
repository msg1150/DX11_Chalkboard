// [칠판 프로젝트에서의 역할]
// 창·입력·렌더러·칠판·UI의 소유자입니다. 각 기능을 실행 순서에 맞게 연결합니다.
// 픽셀 편집과 파일 포맷은 BoardCanvas에 맡기고 이 클래스는 입력 라우팅을 담당합니다.

#pragma once
#include "Features/Chalkboard/BoardScene.h"
#include "Graphics/Renderer.h"
#include "Input/Input.h"
#include "Platform/Window.h"
#include "UI/UIManager.h"
#include "UI/UIRenderer.h"
class Application
{
public:
    bool Initialize(HINSTANCE instance);
    int Run();
    const std::wstring& GetLastErrorMessage()const{return error_;}
private:
    void HandleMessage(HWND hwnd,UINT message,WPARAM w,LPARAM l);
    void Update();
    void Render();
    void ResetCamera();
    Window window_;
    Input input_;
    Renderer renderer_;
    Camera camera_;
    UIManager ui_;
    UIRenderer uiRenderer_;
    BoardScene board_;
    bool ready_=false,rightDrag_=false;
    std::wstring error_;
};
