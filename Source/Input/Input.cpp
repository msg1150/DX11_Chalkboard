// [칠판 프로젝트에서의 역할]
// Win32 메시지를 기능 코드가 사용하기 쉬운 상태와 이벤트로 변환합니다.
// PointerSample은 UI가 각 이벤트 당시 위치와 버튼 상태를 차례대로 검사하도록 만듭니다.

#include "Input/Input.h"
#include <Windowsx.h>
// 지난 키 상태를 보관하고 이번 프레임의 순간 이벤트·이동량·문자 입력을 초기화합니다.
void Input::BeginFrame()
{previous_=keys_;pressed_=released_=lost_=false;dx_=dy_=0;wheel_=0;text_.clear();events_.clear();}
// 메시지 발생 순서대로 Down/Move/Up/Cancel을 기록합니다.
// SetCapture로 창 밖 MouseUp도 받고 포커스/캡처 상실 시 눌림 상태를 정리합니다.
void Input::ProcessMessage(HWND hwnd,UINT message,WPARAM w,LPARAM l)
{
    const auto position=[&]{x_=GET_X_LPARAM(l);y_=GET_Y_LPARAM(l);hasPosition_=true;};
    const auto cancel=[&]
    {
        if(left_)events_.push_back({PointerKind::Cancel,x_,y_,false});
        released_|=left_;left_=right_=false;hasPosition_=false;
    };
    switch(message)
    {
    case WM_KEYDOWN:case WM_SYSKEYDOWN:if(w<keys_.size())keys_[w]=true;break;
    case WM_KEYUP:case WM_SYSKEYUP:if(w<keys_.size())keys_[w]=false;break;
    case WM_KILLFOCUS:lost_=true;keys_.fill(false);text_.clear();cancel();if(GetCapture()==hwnd)ReleaseCapture();break;
    case WM_CANCELMODE:cancel();if(GetCapture()==hwnd)ReleaseCapture();break;
    case WM_CAPTURECHANGED:if(reinterpret_cast<HWND>(l)!=hwnd)cancel();break;
    case WM_LBUTTONDOWN:
        position();pressed_=true;left_=true;events_.push_back({PointerKind::Down,x_,y_,true});SetCapture(hwnd);break;
    case WM_LBUTTONUP:
        position();released_=true;left_=false;events_.push_back({PointerKind::Up,x_,y_,false});
        if(!right_&&GetCapture()==hwnd)ReleaseCapture();break;
    case WM_RBUTTONDOWN:position();right_=true;SetCapture(hwnd);break;
    case WM_RBUTTONUP:position();right_=false;if(!left_&&GetCapture()==hwnd)ReleaseCapture();break;
    case WM_MOUSEMOVE:
        if(hasPosition_){dx_+=GET_X_LPARAM(l)-x_;dy_+=GET_Y_LPARAM(l)-y_;}
        position();events_.push_back({PointerKind::Move,x_,y_,left_});break;
    case WM_MOUSEWHEEL:wheel_+=float(GET_WHEEL_DELTA_WPARAM(w))/WHEEL_DELTA;break;
    case WM_CHAR:if((w>=32&&w<=126)||w==8||w==13)text_.push_back(static_cast<wchar_t>(w));break;
    }
}
// 현재 눌림 상태를 조회합니다. Virtual-Key 배열 범위를 먼저 검사합니다.
bool Input::IsKeyDown(int k)const{return k>=0&&k<256&&keys_[k];}
// 지난 프레임 Up에서 이번 프레임 Down으로 변한 키만 true입니다.
bool Input::IsKeyPressed(int k)const{return k>=0&&k<256&&keys_[k]&&!previous_[k];}
// 지난 프레임 Down에서 이번 프레임 Up으로 변한 키만 true입니다.
bool Input::IsKeyReleased(int k)const{return k>=0&&k<256&&!keys_[k]&&previous_[k];}
// 이벤트 한 개의 위치와 버튼 상태만 담은 UI 입력 샘플을 만듭니다.
// 키 Previous=Current, 휠/문자 비움으로 같은 프레임의 단축키/텍스트를 이벤트마다 중복 처리하지 않습니다.
Input Input::PointerSample(const PointerEvent& e)const
{
    Input sample;
    sample.keys_=keys_;sample.previous_=keys_;
    sample.x_=e.x;sample.y_=e.y;sample.left_=e.left;
    sample.pressed_=e.kind==PointerKind::Down;
    sample.released_=e.kind==PointerKind::Up||e.kind==PointerKind::Cancel;
    sample.lost_=e.kind==PointerKind::Cancel;
    return sample;
}
// 마우스 이벤트를 처리한 뒤 키/문자/휠을 한 번만 처리하는 샘플입니다. Down/Up 순간 플래그는 제거합니다.
Input Input::NonPointerSample()const
{Input sample=*this;sample.events_.clear();sample.pressed_=sample.released_=false;return sample;}
