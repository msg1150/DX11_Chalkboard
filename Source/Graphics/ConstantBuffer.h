// [칠판 프로젝트에서의 역할]
// C++ 상수 구조체를 GPU Constant Buffer로 전송하는 공용 템플릿입니다.
// 칠판은 WVP 행렬과 UseInk 플래그를 Vertex/Pixel Shader의 b0에 연결합니다.

// ============================================================================
// ConstantBuffer.h
// ----------------------------------------------------------------------------
// Shader Constant Buffer를 재사용하기 위한 Template Wrapper.
//
// 예:
// ConstantBuffer<CBTransform>
// ConstantBuffer<CBLight>
//
// Direct3D Constant Buffer는 크기가 반드시 16 Byte 배수여야 한다.
// Template 단계에서 static_assert로 잘못된 구조체 사용을 막는다.
// ============================================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>

template <typename T>
class ConstantBuffer
{
    static_assert(
        sizeof(T) % 16 == 0,
        "Constant Buffer data size must be a multiple of 16 bytes.");

public:
    // Dynamic Constant Buffer를 생성한다.
    // CPU가 매 프레임 Map하여 새로운 값을 넣을 수 있도록 설정한다.
    bool Initialize(ID3D11Device* device)
    {
        D3D11_BUFFER_DESC desc = {};

        desc.ByteWidth =
            sizeof(T);

        desc.Usage =
            D3D11_USAGE_DYNAMIC;

        desc.BindFlags =
            D3D11_BIND_CONSTANT_BUFFER;

        desc.CPUAccessFlags =
            D3D11_CPU_ACCESS_WRITE;

        return SUCCEEDED(
            device->CreateBuffer(
                &desc,
                nullptr,
                buffer_.GetAddressOf()));
    }

    // CPU의 최신 데이터를 GPU Constant Buffer에 복사한다.
    void Update(
        ID3D11DeviceContext* context,
        const T& data)
    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};

        if (FAILED(
            context->Map(
                buffer_.Get(),
                0,
                D3D11_MAP_WRITE_DISCARD,
                0,
                &mapped)))
        {
            return;
        }

        *static_cast<T*>(mapped.pData) =
            data;

        context->Unmap(
            buffer_.Get(),
            0);
    }

    // Vertex Shader의 지정 Slot에 연결한다.
    void BindVS(
        ID3D11DeviceContext* context,
        UINT slot) const
    {
        ID3D11Buffer* buffer =
            buffer_.Get();

        context->VSSetConstantBuffers(
            slot,
            1,
            &buffer);
    }

    // Pixel Shader의 지정 Slot에 연결한다.
    void BindPS(
        ID3D11DeviceContext* context,
        UINT slot) const
    {
        ID3D11Buffer* buffer =
            buffer_.Get();

        context->PSSetConstantBuffers(
            slot,
            1,
            &buffer);
    }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_;
};
