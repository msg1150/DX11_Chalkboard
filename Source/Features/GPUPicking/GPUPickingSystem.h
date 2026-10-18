// [칠판 프로젝트에서의 역할]
// 클릭 위치의 Collider ID와 깊이를 GPU로 구하고 비동기로 읽는 기능입니다.
// 장면 깊이를 복사한 Picking 전용 Z 버퍼에 Proxy 깊이를 기록하므로 가까운 Collider가 남습니다.

// ============================================================================
// GPUPickingSystem.h
// ----------------------------------------------------------------------------
// DirectX11 ID Buffer Picking.
// - R32_UINT Object ID RenderTarget
// - R32_FLOAT 화면 Depth RenderTarget
// - Scene DepthStencil을 전용 Z 버퍼에 복사하여 원본을 보존하고 Proxy에는 Depth Write ON 적용
// - Mouse 1x1 Pixel만 Staging Texture로 복사
// - 3-slot Event Query Ring으로 GPU 완료를 Poll하여 CPU Stall을 피함
// ============================================================================
#pragma once

#include "Features/CollisionEditor/CollisionGeometry.h"
#include "Graphics/ConstantBuffer.h"
#include "Graphics/Shader.h"
#include "Physics/Collision/Core/CollisionWorld.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct GPUPickResult
{
    bool Hit=false;
    Collision::ColliderId ObjectId=0;
    float Depth=1.f;
    DirectX::XMFLOAT3 WorldPosition{};
    std::uint64_t Sequence=0;
};

class GPUPickingSystem
{
public:
    bool Initialize(ID3D11Device* device,const std::filesystem::path& shaderDirectory);

    // 완료된 이전 Readback을 기다리지 않고 확인한다.
    // 아직 GPU가 끝내지 않았으면 std::nullopt를 반환한다.
    std::optional<GPUPickResult> Poll(ID3D11DeviceContext* context);

    // 실제 Picking Pass + 1x1 Copy를 GPU Command Queue에 제출한다.
    // Readback 결과는 Poll()에서 다음 Frame 이후 비동기로 수신한다.
    bool Submit(ID3D11DeviceContext* context,
                ID3D11DepthStencilView* sceneDepth,
                const DirectX::XMMATRIX& view,
                const DirectX::XMMATRIX& projection,
                const std::vector<Collision::ColliderComponent>& colliders,
                int mouseX,int mouseY,UINT width,UINT height);

    const std::wstring& GetError()const{return error_;}
    std::size_t PendingCount()const;

private:
    struct CBTransform
    {
        DirectX::XMFLOAT4X4 WorldViewProjection;
    };
    struct CBPickObject
    {
        std::uint32_t ObjectID=0;
        float Padding[3]={};
    };
    static_assert(sizeof(CBPickObject)==16);

    struct ReadbackSlot
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> IdStaging;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStaging;
        Microsoft::WRL::ComPtr<ID3D11Query> CompletionQuery;
        DirectX::XMFLOAT4X4 InverseViewProjection{};
        int MouseX=0,MouseY=0;
        UINT Width=0,Height=0;
        std::uint64_t Sequence=0;
        bool Pending=false;
    };

    bool EnsureTargets(UINT width,UINT height);
    bool CreateReadbackSlots();
    bool EnsureVertexCapacity(std::size_t vertexCount);
    ReadbackSlot* AcquireFreeSlot();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Shader shader_;
    ConstantBuffer<CBTransform> transformBuffer_;
    ConstantBuffer<CBPickObject> objectBuffer_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> idTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> idRTV_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> depthRTV_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pickingZ_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pickingZView_;

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    std::size_t vertexCapacity_=0;

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthReadOnlyState_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;

    std::array<ReadbackSlot,3> slots_{};
    UINT targetWidth_=0,targetHeight_=0;
    std::uint64_t nextSequence_=1;
    std::wstring error_;
};
