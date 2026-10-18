// [칠판 프로젝트에서의 역할]
// DX11 렌더링 후 대상 Surface를 Direct2D로 넘겨 패널·문자·슬라이더를 그립니다.
// 96 DPI DIP를 사용하여 UI 좌표와 Win32 Client 픽셀 좌표를 맞춥니다.

// ============================================================================
// UIRenderer.cpp : D3D11 칠판 -> D2D UI -> SwapChain Present 순서.
// D2D Target의 BackBuffer 참조는 ResizeBuffers 전에 반드시 해제한다.
// ============================================================================
#include "UI/UIRenderer.h"
#include "UI/UITheme.h"
#include <d2d1helper.h>
#include <Windows.h>
#include <algorithm>

namespace
{
    D2D1_RECT_F ToD2D(const UIRect& r)
    {return D2D1::RectF(r.X,r.Y,r.X+r.W,r.Y+r.H);}
    D2D1_COLOR_F ToD2D(UIColor c)
    {return D2D1::ColorF(c.R,c.G,c.B,c.A);}
}


// 모든 Target이 공통으로 사용하는 Direct2D Factory와 DirectWrite 폰트 형식을 생성합니다.
bool UIRenderer::CreateSharedResources()
{
    if(!factory_)
    {
        const HRESULT hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                           factory_.GetAddressOf());
        if(FAILED(hr)){error_=L"UI: Direct2D Factory 실패";return false;}
    }
    if(!writeFactory_)
    {
        const HRESULT hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(writeFactory_.GetAddressOf()));
        if(FAILED(hr)){error_=L"UI: DirectWrite Factory 실패";return false;}
    }
    const auto makeFormat=[this](float fontSize,DWRITE_FONT_WEIGHT weight,
                                Microsoft::WRL::ComPtr<IDWriteTextFormat>& format)
    {
        if(format)return true;
        const HRESULT result=writeFactory_->CreateTextFormat(
            UITheme::FontFamily,nullptr,weight,DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,fontSize,UITheme::FontLocale,format.GetAddressOf());
        if(SUCCEEDED(result))
        {
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return SUCCEEDED(result);
    };
    if(!makeFormat(UITheme::NormalFontSize,DWRITE_FONT_WEIGHT_NORMAL,textNormal_) ||
       !makeFormat(UITheme::SmallFontSize,DWRITE_FONT_WEIGHT_NORMAL,textSmall_) ||
       !makeFormat(UITheme::HeadingFontSize,DWRITE_FONT_WEIGHT_SEMI_BOLD,textHeading_))
    {error_=L"UI: Text Format 생성 실패";return false;}
    return true;
}

// 실제 앱의 SwapChain BackBuffer를 UI 렌더 대상으로 사용합니다.
bool UIRenderer::Initialize(IDXGISwapChain* swapChain)
{
    error_.clear();
    if(!swapChain){error_=L"UI: SwapChain nullptr";return false;}
    hwnd_=nullptr;
    surface_.Reset();
    hwndTarget_.Reset();
    swapChain_=swapChain;
    if(!CreateSharedResources())return false;
    return CreateTarget();
}

// 별도 Win32 Tool Window에 그릴 때 사용하는 HWND Target 초기화 경로입니다.
bool UIRenderer::InitializeForWindow(HWND hwnd)
{
    error_.clear();
    if(!hwnd){error_=L"UI: HWND nullptr";return false;}
    swapChain_.Reset();
    surface_.Reset();
    hwnd_=hwnd;
    if(!CreateSharedResources())return false;
    return CreateWindowTarget();
}

// HWND Target 크기를 변경하고 장치 리소스 재생성이 필요하면 Target을 다시 만듭니다.
bool UIRenderer::ResizeWindowTarget(unsigned int width,unsigned int height)
{
    if(!hwndTarget_ || width==0u || height==0u)return false;
    const HRESULT hr=hwndTarget_->Resize(D2D1::SizeU(width,height));
    if(SUCCEEDED(hr))return true;
    if(hr==D2DERR_RECREATE_TARGET)
    {
        brush_.Reset();target_.Reset();hwndTarget_.Reset();
        return CreateWindowTarget();
    }
    error_=L"UI: HWND RenderTarget Resize 실패";
    return false;
}

// BackBuffer 참조가 남으면 ResizeBuffers가 실패하므로 Brush와 Target을 먼저 해제합니다.
void UIRenderer::PrepareForResize()
{
    drawing_=false;
    clipDepth_=0;
    brush_.Reset();
    target_.Reset();
    hwndTarget_.Reset();
}

// SwapChain 또는 오프스크린 Surface에 대해 Direct2D Target을 다시 만듭니다.
bool UIRenderer::RecreateTarget()
{
    if((!swapChain_&&!surface_) || !factory_)return false;
    return CreateTarget();
}

// 화면 창 없이 DXGI 텍스처에 같은 UI를 그립니다. 자동 검증의 미리보기 생성에서 사용합니다.
bool UIRenderer::InitializeForSurface(IDXGISurface* surface)
{
    error_.clear();if(!surface){error_=L"UI: DXGI surface nullptr";return false;}
    swapChain_.Reset();hwnd_=nullptr;hwndTarget_.Reset();surface_=surface;
    return CreateSharedResources()&&CreateTarget();
}

// DXGI Surface 위에 고정 96 DPI Target과 단색 Brush를 만듭니다.
bool UIRenderer::CreateTarget()
{
    PrepareForResize();
    Microsoft::WRL::ComPtr<IDXGISurface> surface=surface_;
    if(!surface&&(!swapChain_||FAILED(swapChain_->GetBuffer(0,IID_PPV_ARGS(surface.GetAddressOf())))))
    {error_=L"UI: BackBuffer IDXGISurface 획득 실패";return false;}
    // 고정 96 DPI: Win32 Client Pixel과 D2D DIP를 1:1로 맞춰 HitTest 오차 방지.
    const D2D1_RENDER_TARGET_PROPERTIES properties=D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),
        96.f,96.f);
    const HRESULT hr=factory_->CreateDxgiSurfaceRenderTarget(
        surface.Get(),&properties,target_.GetAddressOf());
    if(FAILED(hr)){error_=L"UI: DXGI Surface RenderTarget 생성 실패";return false;}
    if(FAILED(target_->CreateSolidColorBrush(
        D2D1::ColorF(1.f,1.f,1.f,1.f),brush_.GetAddressOf())))
    {error_=L"UI: SolidColorBrush 생성 실패";PrepareForResize();return false;}
    return true;
}


// HWND의 Client 크기에 맞는 Direct2D Target을 생성합니다.
bool UIRenderer::CreateWindowTarget()
{
    if(!hwnd_ || !factory_)return false;
    brush_.Reset();target_.Reset();hwndTarget_.Reset();
    RECT client{};
    if(!GetClientRect(hwnd_,&client)){error_=L"UI: HWND ClientRect 획득 실패";return false;}
    const UINT width=static_cast<UINT>(std::max<LONG>(1,client.right-client.left));
    const UINT height=static_cast<UINT>(std::max<LONG>(1,client.bottom-client.top));
    const auto properties=D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN,D2D1_ALPHA_MODE_UNKNOWN),96.f,96.f);
    const auto hwndProperties=D2D1::HwndRenderTargetProperties(hwnd_,D2D1::SizeU(width,height),
                                                               D2D1_PRESENT_OPTIONS_NONE);
    const HRESULT hr=factory_->CreateHwndRenderTarget(properties,hwndProperties,
                                                       hwndTarget_.GetAddressOf());
    if(FAILED(hr)){error_=L"UI: HWND Direct2D RenderTarget 생성 실패";return false;}
    target_=hwndTarget_;
    if(FAILED(target_->CreateSolidColorBrush(D2D1::ColorF(1.f,1.f,1.f,1.f),brush_.GetAddressOf())))
    {error_=L"UI: HWND SolidColorBrush 생성 실패";brush_.Reset();target_.Reset();hwndTarget_.Reset();return false;}
    return true;
}

// DX11 출력 바인딩을 풀고 명령을 제출한 다음 같은 Surface에서 Direct2D 그리기를 시작합니다.
bool UIRenderer::Begin(ID3D11DeviceContext* context)
{
    if(!context || !target_ || drawing_)return false;
    // D3D 칠판 렌더링이 끝난 다음 BackBuffer를 D2D와 공유합니다.
    context->OMSetRenderTargets(0,nullptr,nullptr);
    context->Flush();
    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Identity());
    drawing_=true;
    clipDepth_=0;
    return true;
}

// SwapChain 없이 HWND Target에 그리는 경로입니다.
bool UIRenderer::BeginWindow()
{
    if(!hwnd_ || !target_ || drawing_)return false;
    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Identity());
    drawing_=true;
    clipDepth_=0;
    return true;
}

// 남은 Clip을 정리하고 EndDraw 오류를 검사합니다. Target을 다시 만들어야 하는 경우 복구를 시도합니다.
void UIRenderer::End()
{
    if(!drawing_ || !target_)return;
    while(clipDepth_>0u)PopClip(); // 예외적인 조기 종료에도 D2D clip stack 균형 유지.
    drawing_=false;
    const HRESULT result=target_->EndDraw();
    if(result==D2DERR_RECREATE_TARGET)
    {
        if(hwnd_)
        {
            brush_.Reset();target_.Reset();hwndTarget_.Reset();
            CreateWindowTarget();
        }
        else
        {
            PrepareForResize();
            RecreateTarget();
        }
    }
    else if(FAILED(result))error_=L"UI: Direct2D EndDraw 실패";
}
void UIRenderer::SetBrush(UIColor color)
{
    brush_->SetColor(ToD2D(color));
}
void UIRenderer::FillRect(const UIRect& rect,UIColor color)
{
    if(!drawing_ || !brush_)return;
    SetBrush(color);target_->FillRectangle(ToD2D(rect),brush_.Get());
}
void UIRenderer::StrokeRect(const UIRect& rect,UIColor color,float stroke)
{
    if(!drawing_ || !brush_)return;
    SetBrush(color);target_->DrawRectangle(ToD2D(rect),brush_.Get(),stroke);
}
void UIRenderer::FillRoundRect(const UIRect& rect,UIColor color,float radius)
{
    if(!drawing_ || !brush_)return;
    SetBrush(color);target_->FillRoundedRectangle(
        D2D1::RoundedRect(ToD2D(rect),radius,radius),brush_.Get());
}
void UIRenderer::Line(float x1,float y1,float x2,float y2,UIColor color,float stroke)
{
    if(!drawing_ || !brush_)return;
    SetBrush(color);target_->DrawLine(D2D1::Point2F(x1,y1),D2D1::Point2F(x2,y2),
                                      brush_.Get(),stroke);
}
// 본문 영역 밖으로 위젯이 그려지지 않도록 Clip을 쌓고 깊이를 기록합니다.
void UIRenderer::PushClip(const UIRect& rect)
{
    if(!drawing_ || !target_)return;
    target_->PushAxisAlignedClip(ToD2D(rect),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    ++clipDepth_;
}
// PushClip과 짝을 맞춰 Clip을 제거합니다. 빈 스택에서는 호출하지 않습니다.
void UIRenderer::PopClip()
{
    if(!drawing_ || !target_ || clipDepth_==0u)return;
    target_->PopAxisAlignedClip();
    --clipDepth_;
}
// 요청한 폰트 크기로 문자를 그리며 주어진 사각형을 벗어나는 부분은 Clip합니다.
void UIRenderer::Text(const std::wstring& text,const UIRect& rect,UIColor color,UIFont font)
{
    if(!drawing_ || !brush_ || text.empty())return;
    IDWriteTextFormat* format=(font==UIFont::Heading)?textHeading_.Get():
                            (font==UIFont::Small)?textSmall_.Get():textNormal_.Get();
    if(!format)return;
    SetBrush(color);
    target_->DrawTextW(text.c_str(),static_cast<UINT32>(text.size()),format,
                       ToD2D(rect),brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
