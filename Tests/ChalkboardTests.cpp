// [칠판 프로젝트에서의 역할]
// 문서/입력/레이어/파일 손상/픽셀 지우기와 DX11 WARP 렌더링을 검증합니다.
// Preview는 실제 BoardScene과 기존 UI 백엔드를 사용하여 화면 PNG를 만듭니다.

#include "Features/Chalkboard/BoardScene.h"
#include "Features/Chalkboard/ImageFile.h"
#include "Input/Input.h"
#include <Windowsx.h>
#include <wincodec.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace Chalkboard;
using namespace DirectX;
using Microsoft::WRL::ComPtr;
namespace
{
int checks=0;
void Check(bool value,const char* text)
{++checks;if(!value)throw std::runtime_error(text);}
std::uint32_t Pixel(BoardCanvas& c,int x,int y){return c.Composite()[std::size_t(y)*c.Width+std::size_t(x)];}
void Stroke(BoardCanvas& c,Point a,Point b)
{c.BeginEdit();c.Segment(a,b);c.EndEdit();}
void DocumentTests(const std::filesystem::path& root)
{
    BoardCanvas c;c.color=0xFF0000FFu;c.chalkSize=4;
    Stroke(c,{10,10},{1400,600});
    for(int x=10;x<=1400;x+=10)
    {
        const int y=int(10.f+(float(x)-10)*590.f/1390.f);
        Check(Pixel(c,x,y)==0xFF0000FFu,"fast stroke has a gap");
    }
    Check(c.NeedsUpload(),"CPU composite must not consume GPU dirty flag");
    Check(c.Undo()&&Pixel(c,700,303)==0,"stroke undo");
    Check(c.Redo()&&Pixel(c,700,303)!=0,"stroke redo");
    BoardCanvas lower;lower.color=0xFF0000FFu;lower.chalkSize=20;Stroke(lower,{100,100},{300,100});
    BoardCanvas upper;upper.color=0xFF00FFFFu;upper.chalkSize=20;Stroke(upper,{200,50},{200,150});
    std::string error;
    Check(lower.Append(upper,error),"append layer");
    Check(Pixel(lower,200,100)==upper.color,"later import must cover earlier drawing");
    Check(Pixel(lower,120,100)==0xFF0000FFu,"transparent import must preserve lower pixels");
    lower.eraser=true;lower.eraserSize=30;Stroke(lower,{200,100},{200,100});
    for(const auto& layer:lower.Layers())Check(layer[100*lower.Width+200]==0,"erase must clear RGBA in every layer");
    Check(Pixel(lower,200,100)==0,"erased composite must be empty");
    Check(lower.Undo()&&Pixel(lower,200,100)==upper.color,"erase undo restores all layers");
    const auto save=root/L"roundtrip.chalk";
    Check(lower.Save(save,error),"save");
    BoardCanvas loaded;
    loaded.color=0xFFFF0000u;loaded.chalkSize=13.5f;loaded.eraserSize=65.25f;loaded.eraser=true;
    const auto toolsUnchanged=[&]{return loaded.color==0xFFFF0000u&&loaded.chalkSize==13.5f&&loaded.eraserSize==65.25f&&loaded.eraser;};
    Check(loaded.Load(save,error),"load");
    Check(loaded.Layers()==lower.Layers(),"layer order and pixels roundtrip");
    Check(toolsUnchanged(),"opening drawing must preserve all current tools");
    Check(!loaded.Dirty(),"loaded document must be clean");
    Check(loaded.NeedsUpload(),"loaded document must upload even after composite inspection");
    Check(loaded.Composite()==lower.Composite(),"composite roundtrip");
    std::ifstream input(save,std::ios::binary);const std::string savedBytes((std::istreambuf_iterator<char>(input)),{});
    input.close();
    // 도구 설정만 바꾸고 재저장해도 바이트가 같아야 합니다. 그림 파일에 도구 설정이 섞이지 않는지 검사합니다.
    lower.color=0xFF123456u;lower.chalkSize=39.5f;lower.eraserSize=78.25f;lower.eraser=false;
    const auto toolsOnly=root/L"tools-only.chalk";
    Check(lower.Save(toolsOnly,error),"save with different tools");
    std::ifstream toolsInput(toolsOnly,std::ios::binary);
    const std::string toolsBytes((std::istreambuf_iterator<char>(toolsInput)),{});toolsInput.close();
    Check(savedBytes==toolsBytes,"tool changes must not change saved drawing bytes");
    Check(savedBytes.substr(8,4)==std::string("\x02\0\0\0",4),"drawing-only format version");
    BoardCanvas blank;
    const auto blankPath=root/L"blank.chalk";
    Check(blank.Save(blankPath,error)&&std::filesystem::file_size(blankPath)==36,"blank v2 file uses 24-byte header and one RLE run");
    // 구버전 헤더의 분필 RGBA/굵기/지우개 크기 12바이트를 삽입하여 실제 v1 레이아웃을 만듭니다.
    auto legacyBytes=savedBytes;legacyBytes[8]=1;
    legacyBytes.insert(24,std::string("\xFF\0\0\xFF\x14\0\0\0\x1E\0\0\0",12));
    const auto legacy=root/L"legacy-v1.chalk";
    {std::ofstream out(legacy,std::ios::binary);out.write(legacyBytes.data(),std::streamsize(legacyBytes.size()));}
    Check(loaded.Load(legacy,error),"legacy v1 drawing load");
    Check(loaded.Layers()==lower.Layers(),"legacy drawing pixels and order preserved");
    Check(toolsUnchanged(),"legacy stored tools must be ignored");
    Check(loaded.Append(upper,error)&&toolsUnchanged(),"append must also preserve current tools");
    const auto damaged=root/L"damaged.chalk";
    {std::ofstream out(damaged,std::ios::binary);out.write(legacyBytes.data(),35);}
    const auto beforeLegacyFailure=loaded.Layers();
    Check(!loaded.Load(damaged,error)&&loaded.Layers()==beforeLegacyFailure&&toolsUnchanged(),"truncated legacy settings must preserve drawing and tools");
    auto unsupported=savedBytes;unsupported[8]=3;
    {std::ofstream out(damaged,std::ios::binary);out.write(unsupported.data(),std::streamsize(unsupported.size()));}
    Check(!loaded.Load(damaged,error),"unsupported version rejected");
    std::string bytes=savedBytes;
    bytes.back()^=1;{std::ofstream out(damaged,std::ios::binary);out.write(bytes.data(),std::streamsize(bytes.size()));}
    const auto before=loaded.Layers();Check(!loaded.Load(damaged,error),"damaged checksum rejection");
    Check(loaded.Layers()==before,"failed load must preserve current drawing");
    Check(toolsUnchanged(),"failed load must preserve current tools");
    {std::ofstream out(damaged,std::ios::binary);out.write(bytes.data(),19);}
    Check(!loaded.Load(damaged,error),"truncated file rejection");
    bytes[20]=bytes[21]=bytes[22]=bytes[23]='\xFF';
    {std::ofstream out(damaged,std::ios::binary);out.write(bytes.data(),std::streamsize(bytes.size()));}
    Check(!loaded.Load(damaged,error),"unbounded layer count rejection");
    BoardCanvas limit;
    for(unsigned n=0;n<BoardCanvas::MaxLayers-1;++n)Check(limit.Append(upper,error),"layer capacity");
    Check(!limit.Append(upper,error),"layer capacity limit");
    Check(limit.Undo(),"append undo");
    Check(limit.Append(upper,error),"append after undo");
    limit.Clear();Check(limit.LayerCount()==1&&Pixel(limit,200,100)==0,"clear drawing");
    Check(limit.Undo()&&Pixel(limit,200,100)==upper.color,"clear undo");
}
void StrokeTests()
{
    BoardCanvas c;c.chalkSize=4;StrokeTracker tracker;
    tracker.Begin(c,true);tracker.Sample(c,Point{200,100},false);
    tracker.Sample(c,Point{-20,100},false);
    tracker.Sample(c,Point{-20,300},false);
    tracker.Sample(c,Point{200,300},false);tracker.End(c);
    Check(Pixel(c,0,100)!=0&&Pixel(c,0,300)!=0,"draw through exit and entry boundaries");
    Check(Pixel(c,100,200)==0,"must not join exit and entry with a diagonal");
    Check(c.Undo()&&Pixel(c,100,100)==0&&Pixel(c,100,300)==0,"re-entry is one undoable drag");
    tracker.Begin(c,true);tracker.Sample(c,Point{100,100},false);
    tracker.Sample(c,Point{200,200},true);tracker.Sample(c,Point{300,300},false);tracker.End(c);
    Check(Pixel(c,200,200)==0,"UI must break stroke continuity");
    Check(Pixel(c,300,300)!=0,"drawing resumes after leaving UI");
    tracker.Begin(c,false);tracker.Sample(c,Point{500,500},false);tracker.End(c);
    Check(Pixel(c,500,500)==0,"UI-owned drag must never draw");
    tracker.Begin(c,true);tracker.Sample(c,Point{600,600},false);tracker.End(c);
    tracker.Sample(c,Point{700,600},false);Check(Pixel(c,700,600)==0,"cancel/release ends drawing");
    Point a{-100,-100},b{-20,-20};Check(!BoardCanvas::Clip(a,b),"outside segment rejected");
}
void InputTests()
{
    Input input;input.BeginFrame();
    input.ProcessMessage(nullptr,WM_LBUTTONDOWN,0,MAKELPARAM(100,100));
    input.ProcessMessage(nullptr,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(200,100));
    input.ProcessMessage(nullptr,WM_LBUTTONUP,0,MAKELPARAM(300,100));
    Check(input.PointerEvents().size()==3,"same-frame pointer events retained");
    const auto first=input.PointerSample(input.PointerEvents()[0]);
    const auto last=input.PointerSample(input.PointerEvents()[2]);
    Check(first.IsLeftMousePressed()&&first.IsLeftMouseDown()&&first.GetMouseX()==100,"down sample");
    Check(last.IsLeftMouseReleased()&&!last.IsLeftMouseDown()&&last.GetMouseX()==300,"up sample");
    UIManager ui;ui.SetPanelOpen(0,true);ui.Layout(1000,700);
    const auto down=input.PointerSample({Input::PointerKind::Down,800,100,true});
    Check(!ui.Update(down,1000,700),"UI down capture");
    const auto move=input.PointerSample({Input::PointerKind::Move,100,100,true});
    Check(!ui.Update(move,1000,700),"UI drag remains captured outside UI");
    ui.Update(input.PointerSample({Input::PointerKind::Up,100,100,false}),1000,700);
    input.BeginFrame();input.ProcessMessage(nullptr,WM_LBUTTONDOWN,0,MAKELPARAM(10,10));
    input.ProcessMessage(nullptr,WM_KILLFOCUS,0,0);
    Check(!input.IsLeftMouseDown()&&input.DidLoseFocus(),"focus loss releases input");
}
void GpuTests(const std::filesystem::path& directory)
{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
          nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"WARP device");
    constexpr UINT width=640,height=480;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
    desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> output,depth,staging;
    ComPtr<ID3D11RenderTargetView> rtv;ComPtr<ID3D11DepthStencilView> dsv;
    Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&output)),"offscreen texture");
    Check(SUCCEEDED(device->CreateRenderTargetView(output.Get(),nullptr,&rtv)),"offscreen RTV");
    desc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&depth)),"depth texture");
    Check(SUCCEEDED(device->CreateDepthStencilView(depth.Get(),nullptr,&dsv)),"depth view");
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)),"readback texture");
    Camera camera;camera.SetTarget({0,0,0});camera.SetOrbit(XM_PI,0,16.5f);
    camera.SetPerspective(XMConvertToRadians(45.f),float(width)/height,.1f,400);
    BoardScene board;Check(board.Initialize(device.Get(),directory,nullptr),"board resources and HLSL");
    const auto middle=board.Project(320,240,width,height,camera);
    Check(middle&&std::abs(middle->x-768)<5&&std::abs(middle->y-360)<5,"front-face UV projection");
    camera.SetOrbit(0,0,16.5f);Check(!board.Project(320,240,width,height,camera),"backface drawing rejected");
    camera.SetOrbit(XM_PI,0,16.5f);
    const auto render=[&]
    {
        auto* target=rtv.Get();context->OMSetRenderTargets(1,&target,dsv.Get());
        const float bg[]={.6f,.83f,.88f,1};context->ClearRenderTargetView(rtv.Get(),bg);
        context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
        D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);
        board.Render(context.Get(),dsv.Get(),camera,width,height);
        context->CopyResource(staging.Get(),output.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"GPU render readback");
        const auto pixel=*reinterpret_cast<const std::uint32_t*>(static_cast<const BYTE*>(mapped.pData)+240*mapped.RowPitch+320*4);
        context->Unmap(staging.Get(),0);return pixel;
    };
    const auto green=render();Check((green&255)<60&&((green>>8)&255)>140,"green board rendered");
    board.canvas.color=0xFF0000FF;board.canvas.chalkSize=80;Stroke(board.canvas,*middle,*middle);
    board.canvas.Composite(); // Exercise export-before-render cache behavior.
    Check(render()==0xFF0000FF,"ink rendered after CPU composite inspection");
    board.canvas.eraser=true;board.canvas.eraserSize=100;Stroke(board.canvas,*middle,*middle);
    Check(render()==green,"RGBA erase restores original board on GPU");
    GPUPickingSystem picking;Check(picking.Initialize(device.Get(),directory/L"Shaders/GPUPicking"),"picking initialize");
    Collision::CollisionWorld world;const auto nearId=world.AddOBB({0,0,0},{2,2,1});
    world.AddOBB({0,0,5},{2,2,1}); // Deliberately draw farther proxy last.
    context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
    Check(picking.Submit(context.Get(),dsv.Get(),camera.GetViewMatrix(),camera.GetProjectionMatrix(),world.Colliders(),320,240,width,height),"GPU submit");
    context->Flush();std::optional<GPUPickResult> result;
    for(int i=0;i<2000&&!result;++i){result=picking.Poll(context.Get());if(!result)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    Check(result&&result->Hit&&result->ObjectId==nearId,"nearest GPU proxy wins regardless of draw order");
}
void Preview(const std::filesystem::path& directory)
{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
          nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"preview device");
    constexpr UINT width=1440,height=900;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
    desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> output,depth,staging;
    ComPtr<ID3D11RenderTargetView> rtv;ComPtr<ID3D11DepthStencilView> dsv;
    Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&output)),"preview texture");
    Check(SUCCEEDED(device->CreateRenderTargetView(output.Get(),nullptr,&rtv)),"preview RTV");
    desc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&depth)),"preview depth");
    Check(SUCCEEDED(device->CreateDepthStencilView(depth.Get(),nullptr,&dsv)),"preview DSV");
    BoardScene board;Check(board.Initialize(device.Get(),directory,nullptr),"preview board");
    board.canvas.chalkSize=10;board.canvas.color=0xFF3030FFu;
    Stroke(board.canvas,{200,330},{350,170});Stroke(board.canvas,{350,170},{510,140});
    Stroke(board.canvas,{510,140},{650,300});Stroke(board.canvas,{650,300},{1080,355});
    board.canvas.color=0xFF10FFFFu;Stroke(board.canvas,{450,470},{700,310});Stroke(board.canvas,{700,310},{1050,235});
    board.canvas.color=0xFFFFAE00u;Stroke(board.canvas,{650,325},{720,420});
    Stroke(board.canvas,{720,420},{820,420});Stroke(board.canvas,{820,420},{870,180});
    board.canvas.color=0xFFFFFFFFu;board.canvas.MarkSaved();
    Camera camera;camera.SetTarget({2.5f,-.15f,0});camera.SetOrbit(XM_PI,0,16.5f);
    camera.SetPerspective(XMConvertToRadians(45.f),float(width)/height,.1f,400);
    auto* target=rtv.Get();context->OMSetRenderTargets(1,&target,dsv.Get());
    const float bg[]={.6f,.83f,.88f,1};context->ClearRenderTargetView(rtv.Get(),bg);
    context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
    D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);
    board.Render(context.Get(),dsv.Get(),camera,width,height);
    UIManager ui;board.RegisterUI(ui,[]{});ui.Layout(float(width),float(height));
    ComPtr<IDXGISurface> surface;Check(SUCCEEDED(output.As(&surface)),"preview surface");
    UIRenderer renderer;Check(renderer.InitializeForSurface(surface.Get()),"same UI backend on offscreen surface");
    Check(renderer.Begin(context.Get()),"preview UI begin");
    board.RenderOverlay(renderer,float(width),float(height));ui.Render(renderer);renderer.End();
    Check(renderer.GetError().empty(),"preview UI end");
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)),"preview readback");
    context->CopyResource(staging.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"preview map");
    std::vector<std::uint32_t> pixels(std::size_t(width)*height);
    for(UINT y=0;y<height;++y)memcpy(pixels.data()+std::size_t(y)*width,static_cast<BYTE*>(mapped.pData)+y*mapped.RowPitch,width*4);
    context->Unmap(staging.Get(),0);
    const auto path=directory/L"Tests/Artifacts/Preview.png";std::wstring error;
    Check(WritePng(path,width,height,pixels,error),"preview PNG export");
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    Check(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))),"PNG decode WIC");
    Check(SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)),"PNG decoder");
    Check(SUCCEEDED(decoder->GetFrame(0,&frame)),"PNG decoded frame");
    Check(SUCCEEDED(factory->CreateFormatConverter(&converter)),"PNG format converter");
    Check(SUCCEEDED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)),"PNG RGBA conversion");
    std::vector<std::uint32_t> decoded(pixels.size());
    Check(SUCCEEDED(converter->CopyPixels(nullptr,width*4,UINT(decoded.size()*4),reinterpret_cast<BYTE*>(decoded.data()))),"PNG decoded pixels");
    Check(decoded==pixels,"PNG RGB channels must match rendered image");
    const std::vector<std::uint32_t> sample{0,0xFF0000FFu,0xFF00FF00u,0xFFFF0000u};
    const auto transparent=directory/L"Tests/Artifacts/Transparent.png";
    Check(WritePng(transparent,4,1,sample,error),"transparent PNG export");
    converter.Reset();frame.Reset();decoder.Reset();
    Check(SUCCEEDED(factory->CreateDecoderFromFilename(transparent.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)),"transparent PNG decode");
    Check(SUCCEEDED(decoder->GetFrame(0,&frame))&&SUCCEEDED(factory->CreateFormatConverter(&converter)),"transparent PNG frame");
    Check(SUCCEEDED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)),"transparent PNG format");
    std::vector<std::uint32_t> restored(4);
    Check(SUCCEEDED(converter->CopyPixels(nullptr,16,16,reinterpret_cast<BYTE*>(restored.data()))),"transparent PNG pixels");
    Check(restored==sample,"export must preserve cleared RGBA and original RGB colors");
}
}
int wmain(int argc,wchar_t** argv)
{
    try
    {
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        const auto directory=argc>1?std::filesystem::path(argv[1]):std::filesystem::current_path();
        const auto artifacts=directory/L"Tests/Artifacts";std::filesystem::create_directories(artifacts);
        DocumentTests(artifacts);StrokeTests();InputTests();GpuTests(directory);Preview(directory);
        std::cout<<"PASS: "<<checks<<" checks (document, input, boundary/UI continuity, WARP rendering, GPU picking)\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
