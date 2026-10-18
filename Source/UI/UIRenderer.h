// [칠판 프로젝트에서의 역할]
// Direct2D/DirectWrite 공통 UI 백엔드입니다. SwapChain, 별도 HWND, 오프스크린 Surface를 지원합니다.

// ============================================================================
// UIRenderer.h : 프레임워크 공통 2D/UI 그리기 백엔드 (Direct2D+DirectWrite)
// UI 입력/게임 설정/위젯을 전혀 알지 않는다. Pixel 좌표 = 96 DPI DIP 좌표.
// ============================================================================
#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <dxgi.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
struct UIRect
{
    float X=0.f,Y=0.f,W=0.f,H=0.f;
    bool Contains(float x,float y) const noexcept
    { return x>=X && x<X+W && y>=Y && y<Y+H; }
};
struct UIColor
{
    float R=1.f,G=1.f,B=1.f,A=1.f;
};
enum class UIFont {Small,Normal,Heading};
class UIRenderer
{
public:
    bool Initialize(IDXGISwapChain* swapChain);
    // Render the same widgets into an offscreen DXGI texture (preview / QA).
    bool InitializeForSurface(IDXGISurface* surface);
    // 별도 Tool Window용 Direct2D HWND RenderTarget. 게임 SwapChain 경로와 독립적입니다.
    bool InitializeForWindow(HWND hwnd);
    bool ResizeWindowTarget(unsigned int width,unsigned int height);
    void PrepareForResize();
    bool RecreateTarget();
    bool Begin(ID3D11DeviceContext* context);
    // InitializeForWindow()로 만든 Renderer에서만 사용합니다.
    bool BeginWindow();
    void End();
    void FillRect(const UIRect& rect,UIColor color);
    void StrokeRect(const UIRect& rect,UIColor color,float stroke=1.f);
    void FillRoundRect(const UIRect& rect,UIColor color,float radius=6.f);
    void Line(float x1,float y1,float x2,float y2,UIColor color,float stroke=1.f);
    void PushClip(const UIRect& rect);
    void PopClip();
    void Text(const std::wstring& text,const UIRect& rect,UIColor color,
              UIFont font=UIFont::Normal);
    const std::wstring& GetError() const{return error_;}
private:
    bool CreateSharedResources();
    bool CreateTarget();
    bool CreateWindowTarget();
    void SetBrush(UIColor color);
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<IDXGISurface> surface_;
    HWND hwnd_=nullptr;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> writeFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textNormal_,textSmall_,textHeading_;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> hwndTarget_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    bool drawing_=false;
    unsigned int clipDepth_=0;
    std::wstring error_;
};
