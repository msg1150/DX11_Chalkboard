// [칠판 프로젝트에서의 역할]
// CPU 그림 문서와 DX11 화면을 연결하는 칠판 기능입니다.
// Mesh·Collider·Picking·텍스처·기존 UI 바인딩·파일 선택 창을 함께 관리합니다.

#pragma once
#include "Features/Chalkboard/BoardCanvas.h"
#include "Features/GPUPicking/GPUPickingSystem.h"
#include "Graphics/Camera.h"
#include "Graphics/Mesh.h"
#include "Graphics/Shader.h"
#include "Graphics/ConstantBuffer.h"
#include "Physics/Collision/Core/CollisionWorld.h"
#include "UI/UIManager.h"
#include <filesystem>
#include <functional>

class BoardScene
{
public:
    bool Initialize(ID3D11Device* device,const std::filesystem::path& directory,HWND owner);
    void RegisterUI(UIManager& ui,std::function<void()> resetCamera);
    std::optional<Chalkboard::Point> Project(int x,int y,UINT width,UINT height,const Camera& camera)const;
    void RequestPick(int x,int y){pendingPick_=std::pair{x,y};}
    void Render(ID3D11DeviceContext* context,ID3D11DepthStencilView* depth,const Camera& camera,UINT width,UINT height);
    void RenderOverlay(UIRenderer& ui,float width,float height)const;
    // 저장 가능한 원본 문서와, 현재 드래그 상태를 별도로 보관합니다.
    Chalkboard::BoardCanvas canvas;
    Chalkboard::StrokeTracker stroke;
    const std::wstring& Error()const{return error_;}
    void SaveDialog();
    void LoadDialog(bool append);
    void ExportDialog();
    bool ConfirmDiscard()const;
private:
    // HLSL b0와 동일한 레이아웃입니다. UseInk=0은 외형, 1은 그림을 합성하는 초록색 앞면입니다.
    // Constant Buffer의 16바이트 정렬을 맞추기 위해 Padding을 둡니다.
    struct CBScene {DirectX::XMFLOAT4X4 WVP;std::uint32_t UseInk=0,Padding[3]{};};
    static_assert(sizeof(CBScene)==80);
    std::filesystem::path ChooseFile(bool save,bool png=false)const;
    void ShowError(const std::wstring& text)const;
    Shader shader_;
    ConstantBuffer<CBScene> constants_;
    Mesh frame_,face_;
    Collision::CollisionWorld collision_;
    GPUPickingSystem picking_;
    std::optional<std::pair<int,int>> pendingPick_;
    GPUPickResult lastPick_;
    // canvas의 합성 결과를 담는 표시용 텍스처입니다. Save는 이 GPU 텍스처를 읽지 않습니다.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> ink_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> inkView_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState_;
    HWND owner_=nullptr;
    std::filesystem::path directory_,saveDirectory_;
    std::wstring error_,status_=L"초록색 판에 좌클릭 드래그로 그려보세요.";
    bool debugCollider_=false;
};
