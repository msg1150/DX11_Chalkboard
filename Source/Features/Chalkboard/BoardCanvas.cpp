// [칠판 프로젝트에서의 역할]
// 픽셀 편집, 선분 클리핑, 실행 취소, 레이어 합성, .chalk 직렬화를 구현합니다.
// 세부 파일 구조와 UI 호출 흐름은 Docs/SaveLoad.md에도 정리했습니다.

#include "Features/Chalkboard/BoardCanvas.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

namespace Chalkboard
{
namespace
{
constexpr std::size_t PixelCount=std::size_t(BoardCanvas::Width)*BoardCanvas::Height;
// 32비트 정수를 낮은 바이트부터 4바이트로 기록합니다(Little Endian).
// 구조체를 통째로 저장하지 않아 패딩이나 sizeof 차이에 영향을 받지 않습니다.
void Put(std::ostream& out,std::uint32_t v)
{
    for(int i=0;i<4;++i)out.put(static_cast<char>((v>>(i*8))&255u));
}
// Put의 역연산입니다. 4바이트를 전부 읽기 전에 EOF가 나오면 false로 중단합니다.
bool Get(std::istream& in,std::uint32_t& v)
{
    v=0;
    for(int i=0;i<4;++i){const int ch=in.get();if(ch==EOF)return false;v|=std::uint32_t(ch)<<(i*8);}
    return true;
}
// 압축 전 레이어 RGBA 바이트 전체에 FNV-1a 32비트 체크섬을 계산합니다.
// 일반적인 파일 손상 확인용이며 암호화나 악의적인 변조 방지 기능은 아닙니다.
std::uint32_t Hash(const Layer& pixels)
{
    std::uint32_t hash=2166136261u;
    for(auto pixel:pixels)for(int b=0;b<4;++b){hash^=(pixel>>(8*b))&255u;hash*=16777619u;}
    return hash;
}
// 모든 픽셀이 RGBA=0인지 확인합니다. 추가 불러오기에서 빈 작업 레이어를 구분합니다.
bool Empty(const Layer& layer){return std::all_of(layer.begin(),layer.end(),[](auto p){return p==0;});}
}
// 빈 레이어 1개와 같은 크기의 합성 캐시로 시작합니다. GPU 데이터는 아직 없으므로 업로드가 필요합니다.
BoardCanvas::BoardCanvas():layers_(1,Layer(PixelCount)),composite_(PixelCount){}
// 한 번의 좌클릭 드래그를 하나의 편집 단위로 시작합니다. 실제 픽셀이 바뀔 때만 스냅샷을 만듭니다.
void BoardCanvas::BeginEdit(){EndEdit();editing_=true;editRecorded_=false;}
// 현재 편집 묶음을 종료합니다. 다음 드래그는 별도의 실행 취소 단계가 됩니다.
void BoardCanvas::EndEdit(){editing_=false;editRecorded_=false;}
// 픽셀을 바꾸기 직전 상태를 보관합니다. 같은 드래그에서 여러 Segment가 와도 최초 한 번만 복사합니다.
// 새 편집이 발생하면 기존 다시 실행 기록은 현재 작업과 다른 갈래이므로 비웁니다.
void BoardCanvas::BeforeChange()
{
    if(editing_&&editRecorded_)return;
    undo_.push_back(layers_);redo_.clear();TrimHistory();
    if(editing_)editRecorded_=true;
}
// 파일 저장 필요, CPU 합성 재계산 필요, GPU 재업로드 필요를 각각 표시합니다.
void BoardCanvas::Changed(){dirty_=true;compositeDirty_=true;uploadDirty_=true;}
// 최대 8단계 / 128 MiB 안에서 오래된 실행 취소 스냅샷부터 제거합니다.
// 레이어 수가 많으면 스냅샷 하나가 커져 보관할 수 있는 단계 수가 줄어듭니다.
void BoardCanvas::TrimHistory()
{
    constexpr std::size_t limit=128u*1024u*1024u;
    std::size_t bytes=0;
    for(const auto& snapshot:undo_)bytes+=snapshot.size()*PixelCount*4u;
    while(!undo_.empty()&&(undo_.size()>8u||bytes>limit))
    {bytes-=undo_.front().size()*PixelCount*4u;undo_.erase(undo_.begin());}
}
// Liang-Barsky 방식으로 선분의 유효 구간 [t0,t1]을 캔버스 사각형 안으로 자릅니다.
// 판 밖 좌표를 단순히 가장자리로 Clamp하면 테두리를 따라 불필요한 선이 생기므로 선분 자체를 자릅니다.
bool BoardCanvas::Clip(Point& a,Point& b)
{
    if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(b.x)||!std::isfinite(b.y))return false;
    const float dx=b.x-a.x,dy=b.y-a.y;
    float t0=0,t1=1;
    const auto edge=[&](float p,float q)
    {
        if(std::abs(p)<1e-8f)return q>=0;
        const float r=q/p;
        if(p<0){if(r>t1)return false;t0=std::max(t0,r);}
        else{if(r<t0)return false;t1=std::min(t1,r);}
        return true;
    };
    if(!edge(-dx,a.x)||!edge(dx,float(Width)-a.x)||
       !edge(-dy,a.y)||!edge(dy,float(Height)-a.y))return false;
    const Point start=a;
    a={start.x+t0*dx,start.y+t0*dy};b={start.x+t1*dx,start.y+t1*dy};
    return true;
}
// 두 입력점 사이의 선분과 픽셀 중심의 거리를 검사해 굵기가 있는 연속 선을 채웁니다.
// 양 끝에서도 t를 0~1로 제한하므로 끝 모양이 원형이며 빠르게 이동해도 점 사이가 비지 않습니다.
void BoardCanvas::Segment(Point a,Point b)
{
    if(!Clip(a,b))return;
    const float radius=std::clamp(eraser?eraserSize:chalkSize,1.f,160.f)*.5f;
    // 전체 캔버스 대신 선분과 브러시 반지름을 감싸는 사각형 안의 픽셀만 검사합니다.
    const int minX=std::max(0,int(std::floor(std::min(a.x,b.x)-radius)));
    const int maxX=std::min(int(Width)-1,int(std::ceil(std::max(a.x,b.x)+radius)));
    const int minY=std::max(0,int(std::floor(std::min(a.y,b.y)-radius)));
    const int maxY=std::min(int(Height)-1,int(std::ceil(std::max(a.y,b.y)+radius)));
    const float dx=b.x-a.x,dy=b.y-a.y,len2=dx*dx+dy*dy;
    bool touched=false;
    for(int y=minY;y<=maxY;++y)for(int x=minX;x<=maxX;++x)
    {
        const float px=float(x)+.5f,py=float(y)+.5f;
        // 픽셀 중심을 선분에 투영한 최근접 점을 구합니다. len2=0이면 클릭 한 점을 원으로 찍습니다.
        const float t=len2>1e-8f?std::clamp(((px-a.x)*dx+(py-a.y)*dy)/len2,0.f,1.f):0.f;
        const float ex=px-(a.x+t*dx),ey=py-(a.y+t*dy);
        if(ex*ex+ey*ey>radius*radius)continue;
        const std::size_t index=std::size_t(y)*Width+std::size_t(x);
        if(eraser)
        {
            // 초록색 덧칠이 아니라 모든 레이어의 같은 픽셀 값을 0으로 지웁니다. 아래 그림도 남기지 않습니다.
            for(auto& layer:layers_)if(layer[index]!=0)
            {
                if(!touched){BeforeChange();touched=true;}
                layer[index]=0; // Clear all four channels in EVERY layer.
            }
        }
        else if(layers_.back()[index]!=(color|0xFF000000u))
        {
            if(!touched){BeforeChange();touched=true;}
            layers_.back()[index]=color|0xFF000000u;
        }
    }
    if(touched)Changed();
}
// 모든 그림을 빈 레이어 1개로 바꿉니다. 이전 레이어 배열을 보관하므로 모두 지우기도 실행 취소됩니다.
void BoardCanvas::Clear()
{
    EndEdit();
    if(std::all_of(layers_.begin(),layers_.end(),Empty)&&layers_.size()==1)return;
    BeforeChange();layers_.assign(1,Layer(PixelCount));Changed();
}
// 현재 레이어 배열은 Redo 스택으로 옮기고 이전 스냅샷을 복원합니다. 파일/GPU 캐시는 다시 갱신해야 합니다.
bool BoardCanvas::Undo()
{
    EndEdit();if(undo_.empty())return false;
    redo_.push_back(std::move(layers_));layers_=std::move(undo_.back());undo_.pop_back();Changed();return true;
}
// Undo의 반대입니다. 다시 실행할 레이어 상태를 복원하되 스냅샷 메모리 상한을 유지합니다.
bool BoardCanvas::Redo()
{
    EndEdit();if(redo_.empty())return false;
    undo_.push_back(std::move(layers_));layers_=std::move(redo_.back());redo_.pop_back();TrimHistory();Changed();return true;
}
// 검증이 끝난 다른 문서의 그림 레이어를 기존 배열 뒤에 추가합니다(뒤쪽이 화면의 위쪽).
// 원본 other는 변경하지 않으며 기존 분필 색/크기도 유지합니다. 추가 작업 전체를 실행 취소할 수 있습니다.
bool BoardCanvas::Append(const BoardCanvas& other,std::string& error)
{
    error.clear();
    // 완전히 빈 레이어는 가져오지 않습니다. 보이는 그림이 없는 저장본은 오류 안내로 처리합니다.
    std::vector<const Layer*> incoming;
    for(const auto& layer:other.layers_)if(!Empty(layer))incoming.push_back(&layer);
    if(incoming.empty()){error="The file contains no drawing.";return false;}
    // 기존 맨 위가 빈 작업 레이어이면 제거할 예정이므로 개수에서 뺍니다. 마지막 +1은 새 작업 레이어입니다.
    const std::size_t keep=layers_.size()-(Empty(layers_.back())?1u:0u);
    if(keep+incoming.size()+1>MaxLayers){error="Layer limit (24) reached.";return false;}
    // 한 번의 추가 불러오기를 하나의 실행 취소 작업으로 기록한 후에 레이어 배열을 바꿉니다.
    EndEdit();BeforeChange();
    if(Empty(layers_.back()))layers_.pop_back();
    // 기존 아래→위 순서는 그대로 두고 가져온 문서의 아래→위 순서를 배열 뒤에 복사합니다.
    for(const auto* layer:incoming)layers_.push_back(*layer);
    // 새 필기는 불러온 그림보다 위에 있어야 하므로 마지막에 투명 작업 레이어를 하나 추가합니다.
    layers_.emplace_back(PixelCount);Changed();return true;
}
// 아래 레이어부터 순회하여 Alpha가 있는 픽셀만 합성 결과에 덮어씁니다.
// 현재 문서는 Alpha=0 또는 255만 저장하므로 반투명 레이어 간 블렌딩 대신 단순 덮어쓰기로 충분합니다.
// 합성 캐시를 읽었다고 GPU 업로드까지 끝난 것은 아니므로 uploadDirty_는 여기서 내리지 않습니다.
const Layer& BoardCanvas::Composite()
{
    if(compositeDirty_)
    {
        std::fill(composite_.begin(),composite_.end(),0u);
        for(const auto& layer:layers_)for(std::size_t i=0;i<PixelCount;++i)
            if((layer[i]>>24u)!=0)composite_[i]=layer[i];
        compositeDirty_=false;
    }
    return composite_;
}
// 문서를 .chalk 바이너리로 직렬화합니다. 화면 캡처나 GPU Readback을 저장하는 것이 아닙니다.
// 이 함수는 전달받은 경로를 직접 새로 쓰므로 앱에서는 SaveDialog가 만든 임시 경로를 전달합니다.
// 성공 여부만 반환하고 dirty_는 변경하지 않습니다. 최종 파일 교체까지 성공한 호출자가 MarkSaved()를 합니다.
bool BoardCanvas::Save(const std::filesystem::path& path,std::string& error)const
{
    error.clear();
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out){error="Cannot create save file.";return false;}
    // 버전 2의 고정 헤더는 총 24바이트입니다. 그림 해석에 필요한 정보만 기록합니다.
    // [0..7] 식별 문자열 DXCHALK1 / [8..11] 버전 2 / [12..15] 너비 / [16..19] 높이 / [20..23] 레이어 수
    // 식별 문자열은 기존 포맷과 공유하고 별도 버전 필드로 구조를 구별합니다.
    // 식별 문자열을 제외한 모든 필드는 Little Endian uint32입니다. 분필 색/굵기/지우개 크기 등 도구 설정은 저장하지 않습니다.
    out.write("DXCHALK1",8);
    Put(out,2);
    Put(out,Width);
    Put(out,Height);
    Put(out,static_cast<std::uint32_t>(layers_.size()));
    // layers_[0]부터 기록하므로 아래→위의 순서가 파일에서도 유지됩니다.
    // 레이어마다 4바이트 체크섬 다음에 RLE 쌍을 기록합니다. 별도 Run 개수는 저장하지 않습니다.
    for(const auto& layer:layers_)
    {
        Put(out,Hash(layer));
        for(std::size_t i=0;i<PixelCount;)
        {
            const auto pixel=layer[i];
            std::size_t end=i+1;
            // RLE(Run-Length Encoding): 행 우선 배열에서 같은 RGBA 값이 연속된 길이를 구합니다.
            // 예: [0,0,0,빨강,빨강] → (3,0),(2,빨강). 행 경계를 넘어 같은 값이 이어져도 한 Run으로 묶습니다.
            while(end<PixelCount&&layer[end]==pixel)++end;
            // 각 Run은 (반복 횟수 uint32, 픽셀 RGBA uint32) 8바이트입니다. 빈 영역도 값 0으로 압축하여 저장합니다.
            Put(out,static_cast<std::uint32_t>(end-i));
            Put(out,pixel);
            i=end;
        }
    }
    // 버퍼 기록과 파일 닫기까지 성공했는지 검사합니다. 여기의 성공은 직렬화 완료이며 최종 교체는 SaveDialog 담당입니다.
    out.flush();
    if(!out){error="Save write failed.";return false;}
    out.close();
    if(!out){error="Save close failed.";return false;}
    return true;
}
// 파일을 임시 레이어 배열로 끝까지 읽고 검증한 뒤에만 현재 문서에 반영합니다.
// 읽는 도중 오류가 나면 false로 반환하므로 기존 레이어/분필 설정/실행 취소 기록은 그대로 남습니다.
bool BoardCanvas::Load(const std::filesystem::path& path,std::string& error)
{
    error.clear();
    std::ifstream in(path,std::ios::binary);
    const auto invalid=[&]{error="Invalid, damaged or unsupported .chalk file.";return false;};
    if(!in){error="Cannot open save file.";return false;}
    // 확장자만 믿지 않고 파일 내부의 8바이트 식별자를 검사합니다.
    char magic[8]{};
    in.read(magic,8);
    if(std::string(magic,8)!="DXCHALK1")return invalid();
    // 공통 헤더와 지원 버전/고정 해상도/레이어 상한을 먼저 검사한 뒤 픽셀 메모리를 할당합니다.
    std::uint32_t version=0,w=0,h=0,count=0;
    if(!Get(in,version)||!Get(in,w)||!Get(in,h)||!Get(in,count)||
       (version!=1&&version!=2)||w!=Width||h!=Height||count==0||count>MaxLayers)return invalid();
    // 이전 버전 1에는 도구 설정 12바이트가 들어 있습니다. 기존 그림을 읽을 수 있도록 소비만 하고 적용하지 않습니다.
    // 도구 값의 내용은 그림 복원과 무관하지만, 파일이 잘려 이 필드를 끝까지 읽지 못하면 거부합니다.
    if(version==1)
    {
        std::uint32_t ignored=0;
        if(!Get(in,ignored)||!Get(in,ignored)||!Get(in,ignored))return invalid();
    }
    // 트랜잭션용 임시 결과입니다. 파일의 모든 레이어와 끝부분까지 정상이어야 마지막에 layers_로 이동합니다.
    Snapshot loaded;
    for(std::uint32_t n=0;n<count;++n)
    {
        std::uint32_t expected=0;
        if(!Get(in,expected))return invalid();
        Layer layer;
        layer.reserve(PixelCount);
        // 각 레이어를 정확히 Width×Height(1,105,920)개 픽셀까지 복원합니다. 이 개수가 레이어의 종료 기준입니다.
        while(layer.size()<PixelCount)
        {
            std::uint32_t run=0,pixel=0;
            // 잘린 Run, 길이 0, 남은 픽셀 수 초과, 부분 Alpha/숨겨진 RGB 픽셀을 거부합니다.
            // 허용 픽셀은 RGBA 전체가 0인 빈 픽셀 또는 Alpha=255인 분필 픽셀입니다.
            if(!Get(in,run)||!Get(in,pixel)||run==0||run>PixelCount-layer.size()||
               (pixel!=0&&(pixel>>24u)!=255))return invalid();
            // 압축 쌍 (run,pixel)을 동일한 pixel 값 run개로 풀어 행 우선 배열에 추가합니다.
            layer.insert(layer.end(),run,pixel);
        }
        // 압축을 푼 원본 픽셀의 체크섬이 저장 당시 값과 같은지 비교합니다. 체크섬은 헤더를 포함하지 않습니다.
        if(Hash(layer)!=expected)return invalid();
        loaded.push_back(std::move(layer));
    }
    // 선언한 레이어 수를 모두 읽고도 뒤에 데이터가 남으면 현재 버전의 정상 파일로 보지 않습니다.
    if(in.peek()!=EOF)return invalid();
    // 모든 검사가 끝난 유일한 반영 지점입니다. 실행 취소 기록은 파일에 없으므로 새 문서에서 비웁니다.
    EndEdit();
    layers_=std::move(loaded);
    undo_.clear();
    redo_.clear();
    // 현재 도구 설정은 유지합니다. 파일은 방금 읽은 상태이므로 dirty_=false로 둡니다.
    // CPU 합성 캐시와 GPU 텍스처는 이전 문서의 내용이므로 둘 다 다시 갱신해야 합니다.
    dirty_=false;
    compositeDirty_=true;
    uploadDirty_=true;
    return true;
}
// UI가 아닌 장면에서 시작한 드래그에만 필기 소유권을 부여합니다.
void StrokeTracker::Begin(BoardCanvas& canvas,bool sceneOwned)
{End(canvas);active_=sceneOwned;if(active_)canvas.BeginEdit();}
// UI를 통과하거나 앞면 교점이 없으면 연결 기준점을 버립니다.
// 판 밖의 유효한 평면 좌표는 유지하여 선분 클리핑으로 나가는 경계와 재진입 경계를 처리합니다.
void StrokeTracker::Sample(BoardCanvas& canvas,std::optional<Point> point,bool blocked)
{
    if(!active_)return;
    if(blocked||!point){previous_.reset();return;}
    canvas.Segment(previous_.value_or(*point),*point);previous_=point;
}
// 버튼 해제/포커스 상실 등에서 필기와 연결 기준점을 함께 종료합니다.
void StrokeTracker::End(BoardCanvas& canvas)
{if(active_)canvas.EndEdit();active_=false;previous_.reset();}
}
