// [칠판 프로젝트에서의 역할]
// D3D11 장치·컨텍스트·SwapChain·화면 Color/Depth 버퍼의 소유자입니다.

// ============================================================================
// Renderer.h
// ----------------------------------------------------------------------------
// Direct3D 11의 핵심 장치와 Frame Buffer를 관리한다.
//
// Renderer가 담당하는 것:
// - ID3D11Device
// - ID3D11DeviceContext
// - SwapChain
// - RenderTargetView
// - DepthStencil
// - Viewport
// - 기본 Rasterizer State
//
// Mesh나 Camera가 Direct3D 초기화 세부사항을 알지 않도록
// 그래픽 API 기반 코드를 한곳에 모으는 역할을 한다.
// ============================================================================

#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

class Renderer
{
public:
    // Direct3D 11 Device / Context / SwapChain 등을 생성한다.
    bool Initialize(
        HWND hwnd,
        UINT width,
        UINT height);

    // Window 크기가 바뀌었을 때
    // BackBuffer / DepthBuffer / Viewport를 다시 생성한다.
    void Resize(
        UINT width,
        UINT height);

    // RenderTarget / Depth Buffer를 Clear하고 이번 프레임을 시작한다.
    void BeginFrame(
        const float clearColor[4]);

    // 현재 BackBuffer를 화면에 표시한다.
    void EndFrame();

    bool IsInitialized() const;

    // 다른 Graphics 클래스가 GPU 리소스를 생성하거나
    // Pipeline에 바인딩할 때 필요한 Device / Context 접근자.
    ID3D11Device* GetDevice() const;
    ID3D11DeviceContext* GetContext() const;

    // Direct2D Debug Overlay처럼 BackBuffer Surface 접근이 필요한
    // 공용 Graphics 기능을 위해 SwapChain을 읽기 전용으로 노출한다.
    IDXGISwapChain* GetSwapChain() const;

    // GPU Picking처럼 기존 Scene의 Depth 판정 결과를 재사용하는 Feature가
    // 필요할 때 읽기 전용으로 접근한다. 소유권은 Renderer가 계속 유지한다.
    ID3D11DepthStencilView* GetDepthStencilView() const;

    // 현재 BackBuffer 크기. GPU Picking RenderTarget도 같은 크기로 맞춘다.
    UINT GetWidth() const;
    UINT GetHeight() const;

private:
    // SwapChain의 BackBuffer 크기에 종속적인
    // RenderTarget / DepthStencil / Viewport를 생성한다.
    bool CreateBackBufferResources(
        UINT width,
        UINT height);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView_;

    // 현재 기본 Rasterizer:
    // - Solid Fill
    // - Back Face Culling
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;

    UINT width_ = 0;
    UINT height_ = 0;

    bool initialized_ = false;
};
