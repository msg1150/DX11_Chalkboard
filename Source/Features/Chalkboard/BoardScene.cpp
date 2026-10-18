// [칠판 프로젝트에서의 역할]
// 칠판 외형을 생성하고 마우스 Ray의 앞면 교점을 캔버스 좌표로 변환합니다.
// SaveDialog/LoadDialog는 사용자 경로 선택과 작업 교체를 담당하고 실제 파일 해석은 BoardCanvas가 합니다.

#include "Features/Chalkboard/BoardScene.h"
#include "Features/Chalkboard/ImageFile.h"
#include "Features/CollisionEditor/CollisionGeometry.h"
#include "Physics/Query/Raycast.h"
#include <commdlg.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <memory>

using namespace DirectX;
using Microsoft::WRL::ComPtr;
namespace
{
constexpr float HalfWidth=7.f,HalfHeight=3.28125f,FrontZ=-.30f;
std::wstring Wide(const std::string& text){return {text.begin(),text.end()};}
// CollisionGeometry의 OBB 삼각형 생성 기능으로 나무 틀/받침/분필 모양을 만들고 색상을 지정합니다.
void Box(std::vector<Vertex>& all,Collision::Vec3 center,Collision::Vec3 half,XMFLOAT4 color)
{
    Collision::ColliderComponent c;c.Shape=Collision::OBBShape{half};c.Position=center;
    std::vector<Vertex> vertices;CollisionGeometry::BuildSolidTriangles(c,vertices);
    for(auto& v:vertices)v.Color=color;
    all.insert(all.end(),vertices.begin(),vertices.end());
}
// 현재 RGB를 표시하는 읽기 전용 위젯입니다. HitTest=false이므로 직접 클릭 동작은 없습니다.
class ColorPreview final:public UIWidget
{
public:
    explicit ColorPreview(Chalkboard::BoardCanvas& value):canvas_(value){}
    float PreferredHeight()const override{return 32;}
    bool HitTest(float,float)const override{return false;}
    void Render(UIRenderer& ui,bool)const override
    {
        const auto c=canvas_.color;
        ui.FillRoundRect(bounds_,{float(c&255u)/255.f,float((c>>8)&255u)/255.f,float((c>>16)&255u)/255.f,1},4);
        ui.StrokeRect(bounds_,{.8f,.8f,.8f,1});
    }
private:Chalkboard::BoardCanvas& canvas_;
};
}
// 칸의 실제 월드 크기, 화면 Mesh, OBB, Picking, 그림 텍스처와 GPU 상태를 준비합니다.
// 소스 프로젝트 구조를 찾으면 프로젝트 Saves를, 실행 파일만 배포된 구조이면 실행 폴더 Saves를 기본값으로 사용합니다.
bool BoardScene::Initialize(ID3D11Device* device,const std::filesystem::path& directory,HWND owner)
{
    owner_=owner;directory_=directory;
    const auto project=directory.parent_path().parent_path().parent_path();
    saveDirectory_=(std::filesystem::exists(project/L"Chalkboard.vcxproj")?project:directory)/L"Saves";
    std::error_code ec;std::filesystem::create_directories(saveDirectory_,ec);
    if(!shader_.Initialize(device,directory/L"Shaders/Chalkboard/BoardVS.hlsl",directory/L"Shaders/Chalkboard/BoardPS.hlsl"))
    {error_=shader_.GetLastErrorMessage();return false;}
    if(!constants_.Initialize(device)||!picking_.Initialize(device,directory/L"Shaders/GPUPicking"))
    {error_=L"Constant buffer / GPU Picking: "+picking_.GetError();return false;}
    std::vector<Vertex> vertices;
    Box(vertices,{0,0,0},{7.4f,3.68f,.25f},{.70f,.43f,.28f,1});
    Box(vertices,{0,0,-.26f},{7.09f,3.37f,.025f},{.025f,.035f,.025f,1});
    Box(vertices,{0,-3.84f,-.2f},{7.4f,.18f,.5f},{.70f,.43f,.28f,1});
    Box(vertices,{5.9f,-3.61f,-.48f},{.45f,.055f,.055f},{.95f,.95f,.93f,1});
    std::vector<std::uint32_t> indices(vertices.size());std::iota(indices.begin(),indices.end(),0u);
    if(!frame_.Initialize(device,vertices,indices)){error_=L"Frame mesh creation failed";return false;}
    const XMFLOAT4 green{.12f,.65f,.25f,1};
    const std::vector<Vertex> face{
        {{-HalfWidth,HalfHeight,FrontZ},{0,0,-1},{0,0},green},
        {{HalfWidth,HalfHeight,FrontZ},{0,0,-1},{1,0},green},
        {{HalfWidth,-HalfHeight,FrontZ},{0,0,-1},{1,1},green},
        {{-HalfWidth,-HalfHeight,FrontZ},{0,0,-1},{0,1},green}};
    if(!face_.Initialize(device,face,{0,1,2,0,2,3})){error_=L"Board mesh creation failed";return false;}
    collision_.AddOBB({0,0,0},{7.4f,3.68f,.25f});
    collision_.AddOBB({0,-3.84f,-.2f},{7.4f,.18f,.5f});
    collision_.UpdateCollisions();
    D3D11_TEXTURE2D_DESC texture{};texture.Width=canvas.Width;texture.Height=canvas.Height;
    texture.MipLevels=1;texture.ArraySize=1;texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    texture.SampleDesc.Count=1;texture.Usage=D3D11_USAGE_DEFAULT;texture.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    if(FAILED(device->CreateTexture2D(&texture,nullptr,&ink_))||FAILED(device->CreateShaderResourceView(ink_.Get(),nullptr,&inkView_)))
    {error_=L"Ink texture creation failed";return false;}
    D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD=D3D11_FLOAT32_MAX;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depth.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    if(FAILED(device->CreateSamplerState(&sampler,&sampler_))||FAILED(device->CreateRasterizerState(&raster,&rasterizer_))||
       FAILED(device->CreateDepthStencilState(&depth,&depthState_)))
    {error_=L"Board pipeline state creation failed";return false;}
    return true;
}
// 프레임워크 UI를 캔버스 Getter/Setter와 기능 콜백에 연결합니다.
// 열기는 LoadDialog(false), 기존 그림 위 추가는 LoadDialog(true)로 같은 로더를 다른 방식으로 사용합니다.
void BoardScene::RegisterUI(UIManager& ui,std::function<void()> resetCamera)
{
    ui.Add(std::make_unique<UICheckBox>(L"지우개 사용 (모든 레이어)",[this]{return canvas.eraser;},[this](bool v){canvas.eraser=v;}));
    ui.Add(std::make_unique<ColorPreview>(canvas));
    for(int channel=0;channel<3;++channel)
    {
        const int shift=channel*8;
        ui.Add(std::make_unique<UISlider>(channel==0?L"Red":channel==1?L"Green":L"Blue",0.f,255.f,1.f,
            [this,shift]{return float((canvas.color>>shift)&255u);},
            [this,shift](float v){canvas.color=(canvas.color&~(255u<<shift))|(std::uint32_t(std::lround(v))<<shift);}));
    }
    ui.Add(std::make_unique<UISlider>(L"분필 굵기 (캔버스 px)",1.f,80.f,1.f,[this]{return canvas.chalkSize;},[this](float v){canvas.chalkSize=v;}));
    ui.Add(std::make_unique<UISlider>(L"지우개 크기 (캔버스 px)",1.f,160.f,1.f,[this]{return canvas.eraserSize;},[this](float v){canvas.eraserSize=v;}));
    ui.Add(std::make_unique<UIButton>(L"저장 (.chalk)",[this]{SaveDialog();}));
    ui.Add(std::make_unique<UIButton>(L"열기 (작업 전체 복원)",[this]{LoadDialog(false);}));
    ui.Add(std::make_unique<UIButton>(L"추가 불러오기 (맨 위)",[this]{LoadDialog(true);}));
    ui.Add(std::make_unique<UIButton>(L"그림 PNG 내보내기 (투명)",[this]{ExportDialog();}));
    ui.Add(std::make_unique<UIButton>(L"실행 취소 (Ctrl+Z)",[this]{canvas.Undo();}));
    ui.Add(std::make_unique<UIButton>(L"다시 실행 (Ctrl+Y)",[this]{canvas.Redo();}));
    ui.Add(std::make_unique<UIButton>(L"모두 지우기",[this]{canvas.Clear();}));
    ui.Add(std::make_unique<UIButton>(L"카메라 정면으로",std::move(resetCamera)));
    ui.Add(std::make_unique<UICheckBox>(L"Collider 표시",[this]{return debugCollider_;},[this](bool v){debugCollider_=v;}));
    ui.SetPanelOpen(0,true);
}
// Client 픽셀 → NDC → 역 ViewProjection → 월드 Ray → 칠판 앞면 교점 → 캔버스 픽셀 순서입니다.
// 뒤쪽/거의 평행한 Ray와 앞면보다 가까운 가림 물체는 거부합니다.
std::optional<Chalkboard::Point> BoardScene::Project(int x,int y,UINT width,UINT height,const Camera& camera)const
{
    if(width==0||height==0)return {};
    const auto inverse=XMMatrixInverse(nullptr,camera.GetViewMatrix()*camera.GetProjectionMatrix());
    const float nx=2.f*(float(x)+.5f)/float(width)-1.f,ny=1.f-2.f*(float(y)+.5f)/float(height);
    const auto start=XMVector3TransformCoord(XMVectorSet(nx,ny,0,1),inverse);
    const auto end=XMVector3TransformCoord(XMVectorSet(nx,ny,1,1),inverse);
    XMFLOAT3 origin,direction;
    XMStoreFloat3(&origin,start);XMStoreFloat3(&direction,XMVector3Normalize(end-start));
    // Only the front face is writable. Keep unclipped plane coordinates for
    // segment clipping at exit/re-entry, including samples outside the window.
    if(origin.z>=FrontZ||direction.z<1e-5f)return {};
    const float distance=(FrontZ-origin.z)/direction.z;
    if(distance<0)return {};
    const float px=origin.x+direction.x*distance,py=origin.y+direction.y*distance;
    Collision::Ray ray{{origin.x,origin.y,origin.z},{direction.x,direction.y,direction.z}};
    const auto nearest=Collision::Raycast::Closest(ray,collision_.Colliders());
    if(nearest.Hit&&nearest.Distance<distance-.01f)return {}; // e.g. chalk tray occlusion
    return Chalkboard::Point{(px+HalfWidth)/(2*HalfWidth)*canvas.Width,
                             (HalfHeight-py)/(2*HalfHeight)*canvas.Height};
}
// Picking 결과를 확인하고 변경된 CPU 합성 결과만 GPU에 올린 뒤 외형/앞면을 그립니다.
// 그림 원본은 canvas.layers_이고 ink_는 화면 표시용 사본이므로 파일 저장 때 GPU를 읽을 필요가 없습니다.
void BoardScene::Render(ID3D11DeviceContext* context,ID3D11DepthStencilView* depth,const Camera& camera,UINT width,UINT height)
{
    if(auto result=picking_.Poll(context))lastPick_=*result;
    // Pick proxies against the cleared depth before drawing the board. Physics
    // proxy surfaces and the visible front face intentionally have different depths.
    if(pendingPick_&&picking_.Submit(context,depth,camera.GetViewMatrix(),camera.GetProjectionMatrix(),
                                   collision_.Colliders(),pendingPick_->first,pendingPick_->second,width,height))pendingPick_.reset();
    if(canvas.NeedsUpload())
    {
        const auto& pixels=canvas.Composite();
        context->UpdateSubresource(ink_.Get(),0,nullptr,pixels.data(),canvas.Width*4,0);
        canvas.MarkUploaded();
    }
    context->RSSetState(rasterizer_.Get());context->OMSetDepthStencilState(depthState_.Get(),0);
    context->OMSetBlendState(nullptr,nullptr,0xFFFFFFFFu);
    shader_.Bind(context);
    CBScene cb{};XMStoreFloat4x4(&cb.WVP,XMMatrixTranspose(camera.GetViewMatrix()*camera.GetProjectionMatrix()));
    constants_.Update(context,cb);constants_.BindVS(context,0);constants_.BindPS(context,0);
    frame_.Bind(context);frame_.Draw(context);
    cb.UseInk=1;constants_.Update(context,cb);
    auto* view=inkView_.Get();auto* sampler=sampler_.Get();
    context->PSSetShaderResources(0,1,&view);context->PSSetSamplers(0,1,&sampler);
    face_.Bind(context);face_.Draw(context);
    ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);
    if(debugCollider_)
    {
        std::vector<Vertex> lines;
        for(const auto& c:collision_.Colliders())CollisionGeometry::AppendWireframe(c,{1,.85f,.1f,1},lines);
        // Debug geometry is small and only uploaded when the optional overlay is enabled.
        if(!lines.empty())
        {
            ComPtr<ID3D11Device> device;context->GetDevice(&device);
            D3D11_BUFFER_DESC desc{};desc.ByteWidth=UINT(lines.size()*sizeof(Vertex));desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA data{lines.data(),0,0};ComPtr<ID3D11Buffer> buffer;
            if(SUCCEEDED(device->CreateBuffer(&desc,&data,&buffer)))
            {
                cb.UseInk=0;constants_.Update(context,cb);const UINT stride=sizeof(Vertex),offset=0;auto* vb=buffer.Get();
                context->IASetVertexBuffers(0,1,&vb,&stride,&offset);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
                context->Draw(UINT(lines.size()),0);
            }
        }
    }
}
// 조작 안내, 저장 상태, 레이어 수, 마지막 GPU 선택 ID를 표시합니다.
void BoardScene::RenderOverlay(UIRenderer& ui,float width,float height)const
{
    ui.FillRoundRect({12,12,470,62},{.07f,.10f,.12f,.9f},6);
    ui.Text(L"DX11 칠판  |  좌클릭: 필기  ·  우클릭: 회전",{24,18,450,24},{1,1,1,1});
    ui.Text(L"Wheel: 확대/축소  ·  Ctrl+S: 저장  ·  Home: 정면",{24,43,450,20},{.75f,.82f,.85f,1},UIFont::Small);
    ui.FillRoundRect({12,height-70,std::max(0.f,std::min(width-24,760.f)),58},{.07f,.10f,.12f,.90f},6);
    ui.Text(status_,{24,height-65,std::max(0.f,width-48),24},{.9f,.94f,.95f,1},UIFont::Small);
    const auto line=L"레이어 "+std::to_wstring(canvas.LayerCount())+L" / 24  |  "+
        (canvas.Dirty()?L"저장되지 않은 변경":L"저장됨 / 변경 없음")+L"  |  GPU 선택 ID: "+std::to_wstring(lastPick_.ObjectId);
    ui.Text(line,{24,height-40,std::max(0.f,width-48),22},{.65f,.82f,.75f,1},UIFont::Small);
}
// Win32 공통 파일 대화상자에서 경로만 얻습니다. 취소하면 빈 경로를 반환해 파일/문서를 바꾸지 않습니다.
// OFN_NOCHANGEDIR는 대화상자 사용 후 프로세스 작업 폴더가 바뀌는 것을 막습니다.
std::filesystem::path BoardScene::ChooseFile(bool save,bool png)const
{
    wchar_t file[32768]{};
    if(save)wcscpy_s(file,png?L"Drawing.png":L"Board.chalk");
    const auto initial=saveDirectory_.wstring();
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=owner_;
    dialog.lpstrFilter=png?L"Transparent PNG\0*.png\0\0":L"Chalkboard Document\0*.chalk\0\0";
    dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrInitialDir=initial.c_str();
    dialog.lpstrDefExt=png?L"png":L"chalk";
    dialog.Flags=OFN_NOCHANGEDIR|OFN_EXPLORER|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))return file;
    if(CommDlgExtendedError()!=0)ShowError(L"파일 선택 창을 열지 못했습니다.");
    return {};
}
// 저장/읽기/파일 선택 실패를 소유 창에 연결된 오류 창으로 알립니다.
void BoardScene::ShowError(const std::wstring& text)const
{MessageBoxW(owner_,text.c_str(),L"칠판",MB_OK|MB_ICONERROR);}
// 그림에 저장하지 않은 변경이 있을 때 열기/종료 전에 폐기 여부를 확인합니다.
bool BoardScene::ConfirmDiscard()const
{return !canvas.Dirty()||MessageBoxW(owner_,L"저장하지 않은 변경을 버릴까요?",L"칠판",MB_YESNO|MB_ICONQUESTION)==IDYES;}
// 저장 UI의 전체 흐름: 경로 선택 → 같은 폴더의 임시 .tmp 기록 → 최종 파일 교체 → 저장 상태 갱신.
// 원래 파일을 처음부터 잘라 쓰지 않으므로 임시 파일 기록 실패 시 기존 저장본을 건드리지 않습니다.
void BoardScene::SaveDialog()
{
    const auto path=ChooseFile(true);
    if(path.empty())return;
    // 최종 파일과 같은 폴더에 임시 파일을 만듭니다. 같은 위치에서 완성본을 교체하기 위한 경로입니다.
    const auto temporary=std::filesystem::path(path.wstring()+L"."+std::to_wstring(GetTickCount64())+L".tmp");
    std::string error;
    // 픽셀/헤더/RLE 기록에 실패하면 임시 파일만 정리하고 원래 저장본과 현재 그림은 유지합니다.
    if(!canvas.Save(temporary,error)){std::error_code ec;std::filesystem::remove(temporary,ec);ShowError(Wide(error));return;}
    // 임시 파일 작성·닫기 후 Windows 파일 교체를 요청합니다.
    // REPLACE_EXISTING은 기존 저장본 교체, WRITE_THROUGH는 이동 완료까지 기다리도록 요청합니다.
    // 이 방식도 저장 장치 자체의 고장이나 모든 전원 장애에서 복구를 보장하는 백업 기능은 아닙니다.
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    {std::error_code ec;std::filesystem::remove(temporary,ec);ShowError(L"저장 파일 교체 실패. 기존 파일은 유지됩니다.");return;}
    // 최종 교체까지 성공한 뒤에만 저장 완료 표시를 갱신합니다.
    canvas.MarkSaved();
    status_=L"저장 완료: "+path.filename().wstring();
}
// 불러오기 UI의 전체 흐름: 경로 선택 → 별도 BoardCanvas에 파일 검증 → 추가 또는 전체 교체.
// 손상된 파일을 선택했다고 현재 그림을 먼저 지우지 않도록 읽기와 반영을 분리합니다.
void BoardScene::LoadDialog(bool append)
{
    const auto path=ChooseFile(false);
    if(path.empty())return;
    // 현재 canvas와 별도 객체를 만들어 읽습니다. 읽기 성공 전에는 현재 그림을 손대지 않습니다.
    Chalkboard::BoardCanvas loaded;
    // 전체 열기에서 loaded 객체로 교체하더라도 사용 중인 도구 설정이 기본값으로 돌아가지 않도록 유지합니다.
    // 파일 로더는 그림만 복원하며 도구 설정에는 손대지 않습니다(이전 버전 파일도 동일).
    loaded.color=canvas.color;
    loaded.chalkSize=canvas.chalkSize;
    loaded.eraserSize=canvas.eraserSize;
    loaded.eraser=canvas.eraser;
    std::string error;
    if(!loaded.Load(path,error)){ShowError(Wide(error));return;}
    // 추가 모드: 기존 그림/도구 설정을 유지하고 loaded의 그림만 위에 올립니다. 변경된 작업이므로 저장 필요 상태가 됩니다.
    if(append)
    {
        if(!canvas.Append(loaded,error)){ShowError(Wide(error));return;}
    }
    // 전체 열기 모드: 현재 변경 폐기에 동의한 경우 loaded 전체로 교체합니다.
    // 도구 설정은 현재 값을 유지하며 실행 취소 이력은 비워집니다. 전체 열기는 Undo로 되돌리는 작업이 아닙니다.
    else
    {
        if(!ConfirmDiscard())return;
        canvas=std::move(loaded);
    }
    status_=(append?L"맨 위로 추가: ":L"작업 복원: ")+path.filename().wstring();
}
// .chalk와 달리 최종 합성 픽셀만 PNG로 내보냅니다. 개별 레이어는 PNG에 저장하지 않습니다.
// 내보내기는 편집용 저장을 대신하지 않으므로 canvas.MarkSaved()도 호출하지 않습니다.
void BoardScene::ExportDialog()
{
    const auto path=ChooseFile(true,true);if(path.empty())return;
    std::wstring error;
    if(!Chalkboard::WritePng(path,canvas.Width,canvas.Height,canvas.Composite(),error)){ShowError(error);return;}
    status_=L"투명 PNG 내보내기: "+path.filename().wstring();
}
