// [칠판 프로젝트에서의 역할]
// ID/깊이 MRT → 클릭 픽셀 1×1 복사 → 완료 Query 확인 → CPU 결과 복원 순서입니다.
// 연속 필기는 이 비동기 결과를 기다리지 않고 BoardScene의 CPU 교점 계산을 사용합니다.

// ============================================================================
// GPUPickingSystem.cpp
// ============================================================================
#include "Features/GPUPicking/GPUPickingSystem.h"
#include "Graphics/Mesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <utility>

using namespace Collision;
using namespace DirectX;

// Picking Shader와 ID 상수 버퍼, 깊이/래스터 상태, 비동기 Readback 슬롯을 준비합니다.
// 멤버 이름 depthReadOnlyState_는 기존 프레임워크 이름이지만 현재는 전용 깊이 버퍼에 Write ON으로 동작합니다.
bool GPUPickingSystem::Initialize(ID3D11Device* device,const std::filesystem::path& shaderDirectory)
{
    error_.clear();
    if(!device)
    {
        error_=L"GPUPickingSystem: ID3D11Device가 nullptr입니다.";
        return false;
    }
    device_=device;

    if(!shader_.Initialize(device,shaderDirectory/L"PickingVS.hlsl",shaderDirectory/L"PickingPS.hlsl"))
    {
        error_=shader_.GetLastErrorMessage();
        return false;
    }
    if(!transformBuffer_.Initialize(device)||!objectBuffer_.Initialize(device))
    {
        error_=L"GPU Picking ConstantBuffer 생성 실패";
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc={};
    depthDesc.DepthEnable=TRUE;
    depthDesc.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    depthDesc.StencilEnable=FALSE;
    if(FAILED(device->CreateDepthStencilState(&depthDesc,depthReadOnlyState_.GetAddressOf())))
    {
        error_=L"GPU Picking DepthStencilState 생성 실패";
        return false;
    }

    // Collision Shape는 안쪽에서 Camera가 바라보는 경우에도 선택되어야 하므로
    // Picking Pass에서는 Back Face Culling을 사용하지 않는다.
    D3D11_RASTERIZER_DESC rs={};
    rs.FillMode=D3D11_FILL_SOLID;
    rs.CullMode=D3D11_CULL_NONE;
    rs.DepthClipEnable=TRUE;
    // Mouse Pixel 하나만 Rasterize한다. Vertex/Primitive 처리는 그대로지만
    // 불필요한 Pixel Shader/RenderTarget 쓰기를 화면 전체에서 수행하지 않는다.
    rs.ScissorEnable=TRUE;
    if(FAILED(device->CreateRasterizerState(&rs,rasterizerState_.GetAddressOf())))
    {
        error_=L"GPU Picking RasterizerState 생성 실패";
        return false;
    }

    if(!CreateReadbackSlots())return false;
    return true;
}

// 1×1 ID/깊이 Staging과 GPU 완료 Event Query를 3세트 준비해 이전 요청을 기다리지 않고 새 클릭을 처리합니다.
bool GPUPickingSystem::CreateReadbackSlots()
{
    D3D11_TEXTURE2D_DESC idDesc={};
    idDesc.Width=1;idDesc.Height=1;idDesc.MipLevels=1;idDesc.ArraySize=1;
    idDesc.Format=DXGI_FORMAT_R32_UINT;idDesc.SampleDesc.Count=1;
    idDesc.Usage=D3D11_USAGE_STAGING;idDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;

    D3D11_TEXTURE2D_DESC depthDesc=idDesc;
    depthDesc.Format=DXGI_FORMAT_R32_FLOAT;

    D3D11_QUERY_DESC queryDesc={};
    queryDesc.Query=D3D11_QUERY_EVENT;

    for(auto& slot:slots_)
    {
        if(FAILED(device_->CreateTexture2D(&idDesc,nullptr,slot.IdStaging.GetAddressOf()))||
           FAILED(device_->CreateTexture2D(&depthDesc,nullptr,slot.DepthStaging.GetAddressOf()))||
           FAILED(device_->CreateQuery(&queryDesc,slot.CompletionQuery.GetAddressOf())))
        {
            error_=L"GPU Picking 1x1 Readback Ring 생성 실패";
            return false;
        }
    }
    return true;
}

// 화면 크기가 바뀐 경우 ID/깊이 MRT와 Picking 전용 Z 리소스를 재구성합니다.
bool GPUPickingSystem::EnsureTargets(UINT width,UINT height)
{
    if(width==0||height==0)return false;
    if(idTexture_&&depthTexture_&&targetWidth_==width&&targetHeight_==height)return true;

    idRTV_.Reset();idTexture_.Reset();depthRTV_.Reset();depthTexture_.Reset();
    pickingZ_.Reset();pickingZView_.Reset();

    D3D11_TEXTURE2D_DESC desc={};
    desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
    desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;

    desc.Format=DXGI_FORMAT_R32_UINT;
    if(FAILED(device_->CreateTexture2D(&desc,nullptr,idTexture_.GetAddressOf()))||
       FAILED(device_->CreateRenderTargetView(idTexture_.Get(),nullptr,idRTV_.GetAddressOf())))
    {
        error_=L"GPU Picking R32_UINT ID RenderTarget 생성 실패";
        return false;
    }

    desc.Format=DXGI_FORMAT_R32_FLOAT;
    if(FAILED(device_->CreateTexture2D(&desc,nullptr,depthTexture_.GetAddressOf()))||
       FAILED(device_->CreateRenderTargetView(depthTexture_.Get(),nullptr,depthRTV_.GetAddressOf())))
    {
        error_=L"GPU Picking R32_FLOAT Depth RenderTarget 생성 실패";
        return false;
    }

    targetWidth_=width;targetHeight_=height;
    return true;
}

// Proxy 삼각형이 들어갈 크기까지 동적 Vertex Buffer를 2배 단위로 확장합니다.
bool GPUPickingSystem::EnsureVertexCapacity(std::size_t vertexCount)
{
    if(vertexCount==0u)return true;
    if(vertexBuffer_&&vertexCapacity_>=vertexCount)return true;

    std::size_t capacity=512u;
    while(capacity<vertexCount)capacity*=2u;

    D3D11_BUFFER_DESC desc={};
    desc.ByteWidth=static_cast<UINT>(capacity*sizeof(Vertex));
    desc.Usage=D3D11_USAGE_DYNAMIC;
    desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    if(FAILED(device_->CreateBuffer(&desc,nullptr,buffer.GetAddressOf())))
    {
        error_=L"GPU Picking Dynamic VertexBuffer 생성 실패";
        return false;
    }
    vertexBuffer_=std::move(buffer);
    vertexCapacity_=capacity;
    return true;
}

// GPU 처리 중이 아닌 슬롯만 선택합니다. 모두 사용 중이면 Submit이 실패하여 다음 프레임에 재시도합니다.
GPUPickingSystem::ReadbackSlot* GPUPickingSystem::AcquireFreeSlot()
{
    for(auto& slot:slots_)if(!slot.Pending)return &slot;
    return nullptr;
}

// 아직 CPU로 회수하지 못한 Picking 요청 수입니다.
std::size_t GPUPickingSystem::PendingCount()const
{
    return static_cast<std::size_t>(std::count_if(slots_.begin(),slots_.end(),[](const ReadbackSlot& s){return s.Pending;}));
}

// GPU 완료된 슬롯만 Map하므로 CPU가 GPU를 기다리는 일을 줄입니다.
// 여러 요청이 함께 완료되면 Sequence가 가장 큰 최신 선택만 반환하므로 연속 필기용 이벤트 큐로 사용하지 않습니다.
std::optional<GPUPickResult> GPUPickingSystem::Poll(ID3D11DeviceContext* context)
{
    if(!context)return std::nullopt;
    std::optional<GPUPickResult> newest;

    for(auto& slot:slots_)
    {
        if(!slot.Pending)continue;
        BOOL queryDone=FALSE;
        const HRESULT ready=context->GetData(slot.CompletionQuery.Get(),&queryDone,sizeof(queryDone),
                                             D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(ready!=S_OK||queryDone==FALSE)continue;

        D3D11_MAPPED_SUBRESOURCE idMapped={},depthMapped={};
        if(FAILED(context->Map(slot.IdStaging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&idMapped)))continue;
        if(FAILED(context->Map(slot.DepthStaging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&depthMapped)))
        {
            context->Unmap(slot.IdStaging.Get(),0);
            continue;
        }

        const std::uint32_t id=*static_cast<const std::uint32_t*>(idMapped.pData);
        const float depth=*static_cast<const float*>(depthMapped.pData);
        context->Unmap(slot.DepthStaging.Get(),0);
        context->Unmap(slot.IdStaging.Get(),0);

        GPUPickResult result;
        result.Hit=id!=0u;
        result.ObjectId=id;
        result.Depth=depth;
        result.Sequence=slot.Sequence;

        if(result.Hit&&slot.Width>0&&slot.Height>0)
        {
            const float ndcX=2.f*((static_cast<float>(slot.MouseX)+0.5f)/static_cast<float>(slot.Width))-1.f;
            const float ndcY=1.f-2.f*((static_cast<float>(slot.MouseY)+0.5f)/static_cast<float>(slot.Height));
            XMVECTOR clip=XMVectorSet(ndcX,ndcY,depth,1.f);
            const XMMATRIX inverse=XMLoadFloat4x4(&slot.InverseViewProjection);
            XMVECTOR world=XMVector4Transform(clip,inverse);
            const float w=XMVectorGetW(world);
            if(std::abs(w)>1.0e-6f)world=XMVectorScale(world,1.f/w);
            XMStoreFloat3(&result.WorldPosition,world);
        }

        slot.Pending=false;
        if(!newest||result.Sequence>newest->Sequence)newest=result;
    }
    return newest;
}

// 현재 GPU 상태를 보관한 뒤 장면 깊이를 전용 Z에 복사하고 클릭 위치만 Proxy를 렌더링합니다.
// ID/깊이 픽셀을 Staging에 복사하고 Query를 끝낸 후 원래 화면 GPU 상태를 복원합니다.
bool GPUPickingSystem::Submit(ID3D11DeviceContext* context,ID3D11DepthStencilView* sceneDepth,
                              const XMMATRIX& view,const XMMATRIX& projection,
                              const std::vector<ColliderComponent>& colliders,
                              int mouseX,int mouseY,UINT width,UINT height)
{
    if(!context||!sceneDepth||mouseX<0||mouseY<0||
       mouseX>=static_cast<int>(width)||mouseY>=static_cast<int>(height))return false;
    if(!EnsureTargets(width,height))return false;
    Microsoft::WRL::ComPtr<ID3D11Resource> sourceZ;
    sceneDepth->GetResource(&sourceZ);
    if(!pickingZ_)
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> sourceTexture;
        if(FAILED(sourceZ.As(&sourceTexture)))return false;
        D3D11_TEXTURE2D_DESC zDesc{};sourceTexture->GetDesc(&zDesc);
        zDesc.Usage=D3D11_USAGE_DEFAULT;zDesc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        zDesc.CPUAccessFlags=0;zDesc.MiscFlags=0;
        if(FAILED(device_->CreateTexture2D(&zDesc,nullptr,&pickingZ_))||
           FAILED(device_->CreateDepthStencilView(pickingZ_.Get(),nullptr,&pickingZView_)))return false;
    }
    ReadbackSlot* slot=AcquireFreeSlot();
    if(!slot)return false; // 3개의 비동기 Readback이 모두 처리 중이면 다음 Frame에 재시도.

    // 현재 메인 Render Target/State를 보존한다. Picking Feature가 제거되거나
    // 호출 위치가 바뀌어도 다른 Renderer의 Pipeline 상태를 가능한 한 침범하지 않는다.
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRTV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;
    context->OMGetRenderTargets(1,oldRTV.GetAddressOf(),oldDSV.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDepthState;
    UINT oldStencilRef=0;context->OMGetDepthStencilState(oldDepthState.GetAddressOf(),&oldStencilRef);
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> oldRS;
    context->RSGetState(oldRS.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11BlendState> oldBlendState;
    FLOAT oldBlendFactor[4]={};UINT oldSampleMask=0xFFFFFFFFu;
    context->OMGetBlendState(oldBlendState.GetAddressOf(),oldBlendFactor,&oldSampleMask);
    D3D11_VIEWPORT oldViewport={};UINT viewportCount=1;
    context->RSGetViewports(&viewportCount,&oldViewport);
    std::array<D3D11_RECT,D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> oldScissors{};
    UINT oldScissorCount=static_cast<UINT>(oldScissors.size());
    context->RSGetScissorRects(&oldScissorCount,oldScissors.data());

    ID3D11RenderTargetView* targets[2]={idRTV_.Get(),depthRTV_.Get()};
    context->OMSetRenderTargets(0,nullptr,nullptr);
    context->CopyResource(pickingZ_.Get(),sourceZ.Get());
    context->OMSetRenderTargets(2,targets,pickingZView_.Get());
    // Integer ID Target은 Blend 대상이 아니므로 Picking Pass에서는 Blend를 명시적으로 끈다.
    context->OMSetBlendState(nullptr,nullptr,0xFFFFFFFFu);
    const float clearId[4]={0.f,0.f,0.f,0.f};
    const float clearDepth[4]={1.f,1.f,1.f,1.f};
    context->ClearRenderTargetView(idRTV_.Get(),clearId);
    context->ClearRenderTargetView(depthRTV_.Get(),clearDepth);
    context->OMSetDepthStencilState(depthReadOnlyState_.Get(),0);
    context->RSSetState(rasterizerState_.Get());
    const D3D11_VIEWPORT viewport={0.f,0.f,static_cast<float>(width),static_cast<float>(height),0.f,1.f};
    context->RSSetViewports(1,&viewport);
    const D3D11_RECT pickRect={mouseX,mouseY,mouseX+1,mouseY+1};
    context->RSSetScissorRects(1,&pickRect);

    shader_.Bind(context);
    CBTransform transform{};
    XMStoreFloat4x4(&transform.WorldViewProjection,XMMatrixTranspose(view*projection));
    transformBuffer_.Update(context,transform);
    transformBuffer_.BindVS(context,0);

    std::vector<Vertex> triangles;
    for(const auto& collider:colliders)
    {
        if(!collider.Enabled||collider.Id==0u)continue;
        CollisionGeometry::BuildSolidTriangles(collider,triangles);
        if(triangles.empty()||!EnsureVertexCapacity(triangles.size()))continue;

        D3D11_MAPPED_SUBRESOURCE mapped={};
        if(FAILED(context->Map(vertexBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))continue;
        std::copy(triangles.begin(),triangles.end(),static_cast<Vertex*>(mapped.pData));
        context->Unmap(vertexBuffer_.Get(),0);

        const UINT stride=sizeof(Vertex),offset=0;
        ID3D11Buffer* vb=vertexBuffer_.Get();
        context->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        context->IASetIndexBuffer(nullptr,DXGI_FORMAT_UNKNOWN,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        CBPickObject object{};object.ObjectID=collider.Id;
        objectBuffer_.Update(context,object);
        objectBuffer_.BindPS(context,0);
        context->Draw(static_cast<UINT>(triangles.size()),0);
    }

    // 전체 Texture가 아니라 Mouse가 가리킨 1x1 Pixel만 Staging으로 복사한다.
    D3D11_BOX pixelBox={static_cast<UINT>(mouseX),static_cast<UINT>(mouseY),0,
                        static_cast<UINT>(mouseX+1),static_cast<UINT>(mouseY+1),1};
    context->CopySubresourceRegion(slot->IdStaging.Get(),0,0,0,0,idTexture_.Get(),0,&pixelBox);
    context->CopySubresourceRegion(slot->DepthStaging.Get(),0,0,0,0,depthTexture_.Get(),0,&pixelBox);

    const XMMATRIX inverse=XMMatrixInverse(nullptr,view*projection);
    XMStoreFloat4x4(&slot->InverseViewProjection,inverse);
    slot->MouseX=mouseX;slot->MouseY=mouseY;slot->Width=width;slot->Height=height;
    slot->Sequence=nextSequence_++;
    slot->Pending=true;
    context->End(slot->CompletionQuery.Get());

    // 메인 Scene Pipeline 복구.
    ID3D11RenderTargetView* oldTargets[1]={oldRTV.Get()};
    const bool hadOldRTV=(oldRTV.Get()!=nullptr);
    context->OMSetRenderTargets(hadOldRTV?1u:0u,hadOldRTV?oldTargets:nullptr,oldDSV.Get());
    context->OMSetBlendState(oldBlendState.Get(),oldBlendFactor,oldSampleMask);
    context->OMSetDepthStencilState(oldDepthState.Get(),oldStencilRef);
    context->RSSetState(oldRS.Get());
    if(viewportCount>0)context->RSSetViewports(1,&oldViewport);
    if(oldScissorCount>0)context->RSSetScissorRects(oldScissorCount,oldScissors.data());
    return true;
}
