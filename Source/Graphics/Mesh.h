// [칠판 프로젝트에서의 역할]
// 정점/인덱스 데이터를 소유하는 공용 GPU Mesh입니다. 칠판 외형과 앞면이 각각 사용합니다.

// ============================================================================
// Mesh.h
// ----------------------------------------------------------------------------
// GPU Vertex Buffer / Index Buffer를 관리하는 공용 Static Mesh 클래스.
//
// Mesh는 "어떤 물체인지" 알지 못한다.
// 단지 Vertex / Index 데이터를 GPU에 올리고 Draw할 수 있게 한다.
//
// 현재 Vertex는 향후 확장을 고려해 다음 정보를 가진다.
// - Position
// - Normal
// - UV
// - Color
// ============================================================================

#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

struct Vertex
{
    // Object Local 좌표.
    DirectX::XMFLOAT3 Position;

    // Lighting 계산에 사용할 면 방향.
    DirectX::XMFLOAT3 Normal;

    // Texture Mapping 좌표.
    DirectX::XMFLOAT2 UV;

    // 현재 Basic Shader가 사용하는 Vertex Color.
    DirectX::XMFLOAT4 Color;
};

class Mesh
{
public:
    // CPU Vertex / Index 배열을 받아 GPU Buffer를 생성한다.
    bool Initialize(
        ID3D11Device* device,
        const std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices);

    // Input Assembler 단계에 Vertex / Index Buffer를 연결한다.
    void Bind(
        ID3D11DeviceContext* context) const;

    // 저장된 Index 개수만큼 DrawIndexed를 호출한다.
    void Draw(
        ID3D11DeviceContext* context) const;

    // Index Buffer의 일부 연속 범위만 Draw한다.
    //
    // QuadTree Culling뿐 아니라 SubMesh / LOD 등에서도 재사용할 수 있는
    // 범용 Mesh 기능이다.
    void DrawRange(
        ID3D11DeviceContext* context,
        std::uint32_t indexCount,
        std::uint32_t startIndex) const;

    std::uint32_t GetIndexCount() const;

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer_;

    std::uint32_t indexCount_ = 0;
};
