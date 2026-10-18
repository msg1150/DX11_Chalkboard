// [칠판 프로젝트에서의 역할]
// 프레임 시작 시 화면과 깊이를 지우고, 프레임 끝에서 Present합니다.
// 창 크기가 바뀌면 BackBuffer 참조를 해제한 뒤 Color/Depth/Viewport를 재생성합니다.

// ============================================================================
// Renderer.cpp
// ============================================================================

#include "Graphics/Renderer.h"

#include <array>

bool Renderer::Initialize(
    HWND hwnd,
    UINT width,
    UINT height)
{
    width_ = width;
    height_ = height;

    // ------------------------------------------------------------------------
    // SwapChain 설정
    // ------------------------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};

    swapChainDesc.BufferDesc.Width = width_;
    swapChainDesc.BufferDesc.Height = height_;
    swapChainDesc.BufferDesc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    // 현재 MSAA는 사용하지 않는다.
    swapChainDesc.SampleDesc.Count = 1;

    swapChainDesc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT;

    // Double Buffer.
    swapChainDesc.BufferCount = 2;

    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.Windowed = TRUE;

    // 프레임워크 초기 버전에서는 가장 단순한 Swap Effect를 사용한다.
    swapChainDesc.SwapEffect =
        DXGI_SWAP_EFFECT_DISCARD;

    // 가능한 경우 Feature Level 11.1을 사용하고
    // 아니면 11.0으로 내려간다.
    const std::array<D3D_FEATURE_LEVEL, 2> featureLevels =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };

    D3D_FEATURE_LEVEL createdFeatureLevel = {};

    HRESULT hr =
        D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            featureLevels.data(),
            static_cast<UINT>(featureLevels.size()),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            swapChain_.GetAddressOf(),
            device_.GetAddressOf(),
            &createdFeatureLevel,
            context_.GetAddressOf());

    // 일부 시스템은 11.1이 들어간 Feature Level 배열 자체를
    // E_INVALIDARG로 거부할 수 있다.
    // 이 경우 Feature Level 11.0만 전달해서 다시 생성한다.
    if (hr == E_INVALIDARG)
    {
        const D3D_FEATURE_LEVEL fallbackFeatureLevel =
            D3D_FEATURE_LEVEL_11_0;

        hr =
            D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                &fallbackFeatureLevel,
                1,
                D3D11_SDK_VERSION,
                &swapChainDesc,
                swapChain_.GetAddressOf(),
                device_.GetAddressOf(),
                &createdFeatureLevel,
                context_.GetAddressOf());
    }

    if (FAILED(hr))
    {
        return false;
    }

    // RenderTarget / DepthStencil / Viewport 생성.
    if (!CreateBackBufferResources(
            width_,
            height_))
    {
        return false;
    }

    // ------------------------------------------------------------------------
    // 기본 Rasterizer State
    //
    // Back-Face Culling을 켜서 Mesh의 안쪽 면을 그리지 않는다.
    // 따라서 Mesh의 Triangle Winding이 올바르게 구성되어 있어야 한다.
    //
    // FrontCounterClockwise = FALSE:
    // Direct3D 기본 규칙을 사용한다.
    // ------------------------------------------------------------------------
    D3D11_RASTERIZER_DESC rasterizerDesc = {};

    rasterizerDesc.FillMode =
        D3D11_FILL_SOLID;

    rasterizerDesc.CullMode =
        D3D11_CULL_BACK;

    rasterizerDesc.FrontCounterClockwise =
        FALSE;

    rasterizerDesc.DepthClipEnable =
        TRUE;

    hr = device_->CreateRasterizerState(
        &rasterizerDesc,
        rasterizerState_.GetAddressOf());

    if (FAILED(hr))
    {
        return false;
    }

    context_->RSSetState(
        rasterizerState_.Get());

    initialized_ = true;
    return true;
}

void Renderer::Resize(
    UINT width,
    UINT height)
{
    // 초기화 이전이거나 최소화 상태이면 Resize하지 않는다.
    if (!initialized_ ||
        width == 0 ||
        height == 0)
    {
        return;
    }

    width_ = width;
    height_ = height;

    // 기존 BackBuffer를 참조하는 RenderTarget을 먼저 Pipeline에서 해제한다.
    context_->OMSetRenderTargets(
        0,
        nullptr,
        nullptr);

    // ResizeBuffers 전에 기존 BackBuffer 종속 리소스를 반드시 Release한다.
    renderTargetView_.Reset();
    depthStencilView_.Reset();
    depthStencilBuffer_.Reset();

    if (FAILED(
        swapChain_->ResizeBuffers(
            0,
            width_,
            height_,
            DXGI_FORMAT_UNKNOWN,
            0)))
    {
        return;
    }

    CreateBackBufferResources(
        width_,
        height_);
}

void Renderer::BeginFrame(
    const float clearColor[4])
{
    // 이번 프레임에서 사용할 Color / Depth Target을 Pipeline에 연결한다.
    ID3D11RenderTargetView* renderTargets[] =
    {
        renderTargetView_.Get()
    };

    context_->OMSetRenderTargets(
        1,
        renderTargets,
        depthStencilView_.Get());

    // 이전 프레임의 Color를 제거한다.
    context_->ClearRenderTargetView(
        renderTargetView_.Get(),
        clearColor);

    // 이전 프레임의 Depth / Stencil 값을 초기화한다.
    context_->ClearDepthStencilView(
        depthStencilView_.Get(),
        D3D11_CLEAR_DEPTH |
        D3D11_CLEAR_STENCIL,
        1.0f,
        0);
}

void Renderer::EndFrame()
{
    // 첫 번째 인자 1은 VSync를 활성화한다.
    // 모니터 Refresh 주기에 맞춰 Present한다.
    swapChain_->Present(
        1,
        0);
}

bool Renderer::IsInitialized() const
{
    return initialized_;
}

ID3D11Device* Renderer::GetDevice() const
{
    return device_.Get();
}

ID3D11DeviceContext* Renderer::GetContext() const
{
    return context_.Get();
}

IDXGISwapChain* Renderer::GetSwapChain() const
{
    return swapChain_.Get();
}

ID3D11DepthStencilView* Renderer::GetDepthStencilView() const
{
    return depthStencilView_.Get();
}

UINT Renderer::GetWidth() const
{
    return width_;
}

UINT Renderer::GetHeight() const
{
    return height_;
}

bool Renderer::CreateBackBufferResources(
    UINT width,
    UINT height)
{
    // ------------------------------------------------------------------------
    // Render Target 생성
    // ------------------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

    HRESULT hr =
        swapChain_->GetBuffer(
            0,
            IID_PPV_ARGS(
                backBuffer.GetAddressOf()));

    if (FAILED(hr))
    {
        return false;
    }

    hr =
        device_->CreateRenderTargetView(
            backBuffer.Get(),
            nullptr,
            renderTargetView_.GetAddressOf());

    if (FAILED(hr))
    {
        return false;
    }

    // ------------------------------------------------------------------------
    // Depth / Stencil Buffer 생성
    //
    // 3D Mesh의 앞뒤 깊이를 판단하기 위해 Depth Buffer가 필요하다.
    // ------------------------------------------------------------------------
    D3D11_TEXTURE2D_DESC depthDesc = {};

    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;

    depthDesc.Format =
        DXGI_FORMAT_D24_UNORM_S8_UINT;

    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage =
        D3D11_USAGE_DEFAULT;

    depthDesc.BindFlags =
        D3D11_BIND_DEPTH_STENCIL;

    hr =
        device_->CreateTexture2D(
            &depthDesc,
            nullptr,
            depthStencilBuffer_.GetAddressOf());

    if (FAILED(hr))
    {
        return false;
    }

    hr =
        device_->CreateDepthStencilView(
            depthStencilBuffer_.Get(),
            nullptr,
            depthStencilView_.GetAddressOf());

    if (FAILED(hr))
    {
        return false;
    }

    // ------------------------------------------------------------------------
    // Viewport 설정
    //
    // NDC 좌표(-1~1)를 실제 Window Pixel 영역에 매핑한다.
    // ------------------------------------------------------------------------
    D3D11_VIEWPORT viewport = {};

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width =
        static_cast<float>(width);
    viewport.Height =
        static_cast<float>(height);

    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    context_->RSSetViewports(
        1,
        &viewport);

    return true;
}
