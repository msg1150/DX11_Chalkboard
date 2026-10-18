// [칠판 프로젝트에서의 역할]
// 프로그램 진입점. DPI 좌표계를 맞추고 COM을 준비한 뒤 Application을 실행합니다.
// COM은 투명 PNG를 저장하는 Windows Imaging Component(WIC)에 필요합니다.

#include "Core/Application.h"
#include <Windows.h>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int result=0;
    {
        Application app;
        if(!app.Initialize(instance))
        {MessageBoxW(nullptr,app.GetLastErrorMessage().c_str(),L"Chalkboard initialization error",MB_OK|MB_ICONERROR);result=1;}
        else result=app.Run();
    }
    if(SUCCEEDED(com))CoUninitialize();return result;
}
