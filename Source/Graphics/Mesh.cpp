// [칠판 프로젝트에서의 역할]
// CPU 배열을 Vertex/Index Buffer로 만들고 입력 조립 단계에 바인딩하여 그립니다.

// ============================================================================
// Mesh.cpp
// ============================================================================

#include "Graphics/Mesh.h"

#include <algorithm>

bool Mesh::Initialize(
    ID3D11Device* device,
    const std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices)
{
    // 유효하지 않은 입력으로 빈 GPU Buffer를 만들지 않는다.
    if (!device ||
        vertices.empty() ||
        indices.empty())
    {
        return false;
    }

    // ------------------------------------------------------------------------
    // Vertex Buffer 생성
    // ------------------------------------------------------------------------
    D3D11_BUFFER_DESC vertexBufferDesc = {};

    vertexBufferDesc.ByteWidth =
        static_cast<UINT>(
            sizeof(Vertex) *
            vertices.size());

    vertexBufferDesc.Usage =
        D3D11_USAGE_DEFAULT;

    vertexBufferDesc.BindFlags =
        D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem =
        vertices.data();

    HRESULT hr =
        device->CreateBuffer(
            &vertexBufferDesc,
            &vertexData,
            vertexBuffer_.GetAddressOf());

    if (FAILED(hr))
    {
        return false;
    }

    // ------------------------------------------------------------------------
    // Index Buffer 생성
    // ------------------------------------------------------------------------
    D3D11_BUFFER_DESC indexBufferDesc = {};

    indexBufferDesc.ByteWidth =
        static_cast<UINT>(
            sizeof(std::uint32_t) *
            indices.size());

    indexBufferDesc.Usage =
        D3D11_USAGE_DEFAULT;

    indexBufferDesc.BindFlags =
        D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem =
        indices.data();

    hr =
        device->CreateBuffer(
            &indexBufferDesc,
            &indexData,
            indexBuffer_.GetAddressOf());

    if (FAILED(hr))
    {
        return false;
    }

    indexCount_ =
        static_cast<std::uint32_t>(
            indices.size());

    return true;
}

void Mesh::Bind(
    ID3D11DeviceContext* context) const
{
    // Vertex 하나의 Byte 크기.
    const UINT stride =
        sizeof(Vertex);

    // Buffer 시작 위치.
    const UINT offset = 0;

    ID3D11Buffer* vertexBuffer =
        vertexBuffer_.Get();

    // Input Assembler에 Vertex Buffer 연결.
    context->IASetVertexBuffers(
        0,
        1,
        &vertexBuffer,
        &stride,
        &offset);

    // Index는 uint32_t이므로 R32_UINT Format을 사용한다.
    context->IASetIndexBuffer(
        indexBuffer_.Get(),
        DXGI_FORMAT_R32_UINT,
        0);

    // 현재 모든 Mesh는 Triangle List 방식으로 그린다.
    context->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Mesh::Draw(
    ID3D11DeviceContext* context) const
{
    context->DrawIndexed(
        indexCount_,
        0,
        0);
}

void Mesh::DrawRange(
    ID3D11DeviceContext* context,
    std::uint32_t indexCount,
    std::uint32_t startIndex) const
{
    if (!context ||
        indexCount == 0u ||
        startIndex >= indexCount_)
    {
        return;
    }

    // 잘못된 외부 범위가 들어와도 Index Buffer 끝을 넘지 않도록 제한한다.
    const std::uint32_t safeIndexCount =
        std::min(
            indexCount,
            indexCount_ -
            startIndex);

    context->DrawIndexed(
        safeIndexCount,
        startIndex,
        0);
}

std::uint32_t Mesh::GetIndexCount() const
{
    return indexCount_;
}
