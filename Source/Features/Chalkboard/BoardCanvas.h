// [칠판 프로젝트에서의 역할]
// 칠판 그림의 원본 데이터와 편집 API입니다. 레이어는 아래쪽부터 순서대로 저장합니다.
// 마우스 좌표나 DirectX 장치에 의존하지 않아 파일 저장과 픽셀 편집을 독립적으로 검사할 수 있습니다.

#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// CPU document is authoritative. A zero pixel has no RGB or alpha data.
// GPU textures are only a display cache; files never include the green board.
namespace Chalkboard
{
struct Point { float x=0, y=0; };
// 행 우선 픽셀 배열: index = y * Width + x.
// 정수 표현은 0xAABBGGRR이며 Little Endian 메모리 바이트 순서는 R,G,B,A입니다.
// 현재 편집기는 빈 픽셀(전체 0)과 불투명 픽셀(Alpha 255)만 원본에 저장합니다.
using Layer = std::vector<std::uint32_t>;
class BoardCanvas
{
public:
    static constexpr std::uint32_t Width=1536, Height=720, MaxLayers=24;
    BoardCanvas();
    // 분필/지우개의 크기는 화면이 아닌 고정 캔버스의 픽셀 단위입니다.
    std::uint32_t color=0xFFFFFFFFu;
    float chalkSize=10.f, eraserSize=48.f;
    bool eraser=false;
    void BeginEdit();
    void EndEdit();
    void Segment(Point a,Point b);
    void Clear();
    bool Undo();
    bool Redo();
    // Append는 그림 레이어만 추가하고 현재 도구 설정은 유지합니다.
    bool Append(const BoardCanvas& other,std::string& error);
    // Save/Load는 파일 포맷만 담당합니다. 대화상자·폐기 확인·임시 파일 교체는 BoardScene 담당입니다.
    // 그림 레이어만 저장/복원하며 현재 도구 설정(color/chalkSize/eraserSize/eraser)은 유지합니다.
    bool Save(const std::filesystem::path& path,std::string& error)const;
    bool Load(const std::filesystem::path& path,std::string& error);
    // 화면/PNG 공용 합성 결과입니다. 레이어별 원본과 저장 순서는 별도로 유지됩니다.
    const Layer& Composite();
    const std::vector<Layer>& Layers()const{return layers_;}
    bool Dirty()const{return dirty_;}
    // 실제 최종 파일 교체 성공 후 호출합니다. PNG 내보내기에서는 호출하지 않습니다.
    void MarkSaved(){dirty_=false;}
    bool NeedsUpload()const{return uploadDirty_;}
    void MarkUploaded(){uploadDirty_=false;}
    std::size_t LayerCount()const{return layers_.size();}
    static bool Clip(Point& a,Point& b);
private:
    using Snapshot=std::vector<Layer>;
    void BeforeChange();
    void Changed();
    void TrimHistory();
    // 배열의 앞이 아래, 뒤가 위입니다. 분필은 항상 back()에 쓰고 지우개는 모든 레이어를 비웁니다.
    std::vector<Layer> layers_;
    // 실행 취소 전용 메모리이며 .chalk 파일에는 기록하지 않습니다.
    std::vector<Snapshot> undo_,redo_;
    // 합성/화면 표시를 위한 재생성 가능한 캐시입니다. 문서 원본으로 사용하지 않습니다.
    Layer composite_;
    // editing_: 편집 묶음 진행 / editRecorded_: 해당 묶음의 최초 스냅샷 생성 여부.
    // dirty_: 저장 필요 / compositeDirty_: CPU 합성 필요 / uploadDirty_: GPU 텍스처 갱신 필요.
    bool editing_=false,editRecorded_=false,dirty_=false,compositeDirty_=true,uploadDirty_=true;
};

// Keeps drag ownership even outside the board, but never connects across UI.
class StrokeTracker
{
public:
    void Begin(BoardCanvas& canvas,bool sceneOwned);
    void Sample(BoardCanvas& canvas,std::optional<Point> point,bool blocked);
    void End(BoardCanvas& canvas);
    bool Active()const{return active_;}
private:
    bool active_=false;
    // UI 통과 시 비워 연결을 끊습니다. 판 밖의 유효한 평면 좌표는 클리핑을 위해 보관합니다.
    std::optional<Point> previous_;
};
}
