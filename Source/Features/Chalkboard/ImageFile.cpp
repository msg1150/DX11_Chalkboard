// [칠판 프로젝트에서의 역할]
// WIC 인코더에 맞게 RGBA를 BGRA로 변환한 뒤 PNG를 완성합니다.
// 임시 파일을 먼저 기록하고 스트림을 닫은 후 최종 파일로 교체합니다.

#include "Features/Chalkboard/ImageFile.h"
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <limits>
namespace Chalkboard
{
// PNG 파일에는 픽셀만 저장합니다. 레이어 파일(.chalk)의 직렬화와는 별도 경로입니다.
// 크기 검사 → 채널 변환 → 임시 PNG 인코딩 → 스트림 닫기 → 최종 경로 교체 순서로 진행합니다.
bool WritePng(const std::filesystem::path& path,std::uint32_t width,std::uint32_t height,
              const std::vector<std::uint32_t>& rgba,std::wstring& error)
{
    using Microsoft::WRL::ComPtr;
    error.clear();
    if(width==0||height==0||std::uint64_t(width)*height!=rgba.size()||rgba.size()>std::numeric_limits<UINT>::max()/4u)
    {error=L"PNG 크기가 올바르지 않습니다.";return false;}
    // WIC PNG encoders negotiate BGRA. Convert explicitly rather than assuming
    // that requesting RGBA leaves the negotiated format unchanged.
    // CPU 정수는 0xAABBGGRR이고 Little Endian 바이트는 R,G,B,A입니다.
    // WIC에 전달할 B,G,R,A 배열을 만들기 위해 R과 B 바이트만 교환하고 Alpha는 보존합니다.
    std::vector<std::uint32_t> bgra;bgra.reserve(rgba.size());
    for(const auto p:rgba)bgra.push_back((p&0xFF00FF00u)|((p&255u)<<16u)|((p>>16u)&255u));
    const auto temporary=std::filesystem::path(path.wstring()+L"."+std::to_wstring(GetTickCount64())+L".tmp");
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
    // WIC Factory → 파일 Stream → PNG Encoder → Frame의 순서로 리소스를 생성합니다.
    // 이전 단계 실패 후 다음 단계를 호출하지 않도록 HRESULT를 이어서 검사합니다.
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);
    if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(temporary.c_str(),GENERIC_WRITE);
    if(SUCCEEDED(hr))hr=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);
    if(SUCCEEDED(hr))hr=encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache);
    if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(&frame,nullptr);
    if(SUCCEEDED(hr))hr=frame->Initialize(nullptr);
    if(SUCCEEDED(hr))hr=frame->SetSize(width,height);
    // SetPixelFormat은 요청한 형식을 바꿔 반환할 수 있으므로 실제 협상 결과가 BGRA인지 확인합니다.
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&format);
    if(SUCCEEDED(hr)&&format!=GUID_WICPixelFormat32bppBGRA)hr=E_FAIL;
    if(SUCCEEDED(hr))hr=frame->WritePixels(height,width*4,UINT(bgra.size()*4),reinterpret_cast<BYTE*>(bgra.data()));
    // 프레임 픽셀과 PNG 컨테이너를 각각 Commit해야 완성된 PNG가 됩니다.
    if(SUCCEEDED(hr))hr=frame->Commit();
    if(SUCCEEDED(hr))hr=encoder->Commit();
    // 파일 핸들을 닫은 뒤 이동해야 Windows 공유/잠금 충돌 없이 최종 파일로 교체할 수 있습니다.
    frame.Reset();encoder.Reset();stream.Reset();
    if(SUCCEEDED(hr)&&!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))hr=HRESULT_FROM_WIN32(GetLastError());
    if(FAILED(hr))
    {
        std::error_code ec;std::filesystem::remove(temporary,ec);
        error=L"PNG 내보내기에 실패했습니다. 기존 파일은 유지됩니다.";return false;
    }
    return true;
}
}
