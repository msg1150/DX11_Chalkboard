// [칠판 프로젝트에서의 역할]
// 위젯 렌더링과 클릭/드래그/숫자 편집 동작입니다.
// Button은 PointerUp 시 콜백을 실행하므로 저장·불러오기 창도 여기서 간접 호출됩니다.

// ============================================================================
// UIWidget.cpp : Label / CheckBox / Slider / Panel 그리기와 마우스 반응.
// ============================================================================
#include "UI/UIWidget.h"
#include "UI/UISliderMath.h"
#include "UI/UINumericValue.h"
#include "UI/UITheme.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cstdio>
#include <utility>
namespace
{
    constexpr UIColor kText=UITheme::Text;
    constexpr UIColor kMuted=UITheme::Muted;
    constexpr UIColor kAccent=UITheme::Accent;
}
// 동적 Getter가 있으면 최신 값을, 없으면 고정 문자열을 출력합니다.
void UILabel::Render(UIRenderer& r,bool)const
{
    r.Text(getter_ ? getter_() : text_,
           {bounds_.X,bounds_.Y,bounds_.W,bounds_.H},kMuted,UIFont::Small);
}
UICheckBox::UICheckBox(std::wstring label,std::function<bool()> getter,
                       std::function<void(bool)> setter)
    :label_(std::move(label)),getter_(std::move(getter)),setter_(std::move(setter)){}
void UICheckBox::Render(UIRenderer& r,bool hovered)const
{
    if(hovered)r.FillRoundRect(bounds_,UITheme::RowHover,4.f);
    const float left=bounds_.X+7.f,top=bounds_.Y+5.f;
    const UIRect square={left,top,18.f,18.f};
    const bool checked=getter_();
    r.FillRoundRect(square,checked?kAccent:UITheme::CheckOff,3.f);
    r.StrokeRect(square,checked?kAccent:UITheme::CheckBorder,1.f);
    if(checked)
    {
        r.Line(left+4.f,top+9.f,left+8.f,top+13.f,UITheme::CheckMark,2.f);
        r.Line(left+8.f,top+13.f,left+15.f,top+5.f,UITheme::CheckMark,2.f);
    }
    r.Text(label_,{left+28.f,bounds_.Y+1.f,bounds_.W-42.f,26.f},kText);
}
// 버튼을 놓은 위치가 위젯 안일 때만 Getter의 반대 값을 Setter에 전달합니다.
void UICheckBox::PointerUp(float x,float y)
{
    if(HitTest(x,y))setter_(!getter_());
}
UISlider::UISlider(std::wstring label,float minimum,float maximum,float step,
                  std::function<float()> getter,std::function<void(float)> setter)
    :label_(std::move(label)),min_(minimum),max_(maximum),step_(step),
     getter_(std::move(getter)),setter_(std::move(setter)){}

UIRect UISlider::NumberRect()const
{
    // 슬라이더의 우측 숫자를 클릭하면 텍스트 편집이 시작된다.
    return {bounds_.X+bounds_.W-77.f,bounds_.Y+1.f,69.f,24.f};
}

// 현재 값을 임시 문자열에 복사하여 편집합니다. 첫 입력은 기존 문자열 전체를 대체합니다.
void UISlider::BeginTextEdit()
{
    wchar_t buffer[48]={};
    swprintf_s(buffer,L"%.0f",static_cast<double>(getter_()));
    editText_=buffer;
    selectAll_=true;
    editing_=true;
}

// 슬라이더 트랙의 X 위치를 범위/step에 맞는 값으로 바꾸고 실제 값이 다를 때만 Setter를 호출합니다.
void UISlider::SetFromMouse(float x)
{
    const float left=bounds_.X+10.f;
    const float width=std::max(1.f,bounds_.W-20.f);
    const float candidate=UISliderMath::ValueFromX(x,left,width,min_,max_,step_);
    if(std::abs(candidate-getter_())>0.0001f)setter_(candidate);
}

// 우측 숫자 영역이면 텍스트 편집을, 트랙이면 마우스 값 조절을 시작합니다.
void UISlider::PointerDown(float x,float y)
{
    if(NumberRect().Contains(x,y))
    {
        BeginTextEdit();
        return;
    }
    editing_=false;
    SetFromMouse(x);
}

void UISlider::PointerDrag(float x,float)
{
    // 숫자 입력칸에서 시작한 Drag가 아래의 슬라이더를 조작하지 않도록 방어.
    if(!editing_)SetFromMouse(x);
}

// 숫자·부호·소수점·Backspace·Enter만 처리하며 길이/중복 소수점을 제한합니다.
void UISlider::HandleTextCharacter(wchar_t ch)
{
    if(!editing_)return;
    if(ch==L'\r')
    {
        CommitTextEdit();
        return;
    }
    if(ch==L'\b')
    {
        if(selectAll_)editText_.clear();
        else if(!editText_.empty())editText_.pop_back();
        selectAll_=false;
        return;
    }
    const bool digit=ch>=L'0' && ch<=L'9';
    const bool sign=(ch==L'-'||ch==L'+');
    const bool dot=ch==L'.';
    if(!digit && !sign && !dot)return;
    if(selectAll_)
    {
        editText_.clear();
        selectAll_=false;
    }
    if(editText_.size()>=16)return;
    if(sign && !editText_.empty())return;
    if(dot && editText_.find(L'.')!=std::wstring::npos)return;
    editText_.push_back(ch);
}

// 유효한 숫자만 허용 범위로 제한해 Setter에 적용합니다. 잘못된 문자열은 기존 값을 바꾸지 않습니다.
void UISlider::CommitTextEdit()
{
    if(!editing_)return;
    float parsed=0.f;
    // 잘못된 입력은 setter 호출 없이 폐기한다. 유효하면 범위로 Clamp.
    if(UINumericValue::ParseClamped(editText_,min_,max_,parsed) &&
       std::abs(parsed-getter_())>0.0001f)
        setter_(parsed);
    editing_=false;
    selectAll_=false;
    editText_.clear();
}

// 입력 버퍼만 비우고 바인딩된 값은 유지합니다.
void UISlider::CancelTextEdit()
{
    // Esc, 창 포커스 상실: 원래 바인딩된 설정값을 그대로 유지한다.
    editing_=false;
    selectAll_=false;
    editText_.clear();
}

// 편집 여부에 따라 실제 값 또는 임시 입력 문자열을 그리고 값 비율에 맞춰 트랙/손잡이를 표시합니다.
void UISlider::Render(UIRenderer& r,bool hovered)const
{
    const float value=getter_();
    wchar_t buffer[48]={};
    swprintf_s(buffer,L"%.0f",static_cast<double>(value));
    r.Text(label_,{bounds_.X+8.f,bounds_.Y,bounds_.W-87.f,25.f},kText);

    // 텍스트 편집 중에는 강조된 사각형과 입력 버퍼를 출력한다.
    // 실제 바인딩 값은 Enter 또는 입력칸 밖 클릭 전까지 변경하지 않는다.
    const UIRect field=NumberRect();
    r.FillRoundRect(field,editing_?UITheme::InputFocused:
                            UITheme::InputNormal,4.f);
    r.StrokeRect(field,editing_?kAccent:UITheme::InputBorder,1.f);
    const std::wstring display=editing_?editText_:std::wstring(buffer);
    r.Text(display+(editing_?L"|":L""),
           {field.X+6.f,field.Y+1.f,field.W-9.f,field.H-2.f},
           editing_?kText:kAccent,UIFont::Normal);

    const float x=bounds_.X+10.f,y=bounds_.Y+39.f,width=std::max(1.f,bounds_.W-20.f);
    r.FillRoundRect({x,y,width,7.f},UITheme::SliderTrack,3.f);
    const float fraction=UISliderMath::Fraction(value,min_,max_);
    r.FillRoundRect({x,y,std::max(2.f,width*fraction),7.f},kAccent,3.f);
    const float knob=x+width*fraction;
    r.FillRoundRect({knob-5.f,y-5.f,10.f,17.f},
                    hovered?UITheme::SliderKnobHover:UITheme::SliderKnob,4.f);
    wchar_t leftLabel[32]={},rightLabel[32]={};
    swprintf_s(leftLabel,L"최소 %.0f",static_cast<double>(min_));
    swprintf_s(rightLabel,L"최대 %.0f",static_cast<double>(max_));
    r.Text(leftLabel,{bounds_.X+9.f,bounds_.Y+49.f,85.f,15.f},kMuted,UIFont::Small);
    r.Text(rightLabel,{bounds_.X+bounds_.W-79.f,bounds_.Y+49.f,75.f,15.f},
           kMuted,UIFont::Small);
}
void UIButton::Render(UIRenderer& renderer,bool hovered)const
{
    const UIColor background=hovered ? UITheme::ButtonHover : UITheme::ButtonNormal;
    renderer.FillRoundRect(bounds_,background,7.f);
    renderer.StrokeRect(bounds_,kAccent,1.f);
    renderer.Text(label_,{bounds_.X+11.f,bounds_.Y+1.f,bounds_.W-15.f,bounds_.H-2.f},
                  kText,UIFont::Normal);
}
// 위젯 안에서 클릭을 놓았을 때 저장/불러오기 등 등록된 기능 콜백을 호출합니다.
void UIButton::PointerUp(float x,float y)
{
    if(HitTest(x,y) && onClick_)onClick_();
}

// 배경·테두리·제목·구분선을 그립니다. 본문 위젯의 Clip/스크롤은 UIManager가 처리합니다.
void UIPanel::Render(UIRenderer& renderer,const std::wstring& title)const
{
    renderer.FillRoundRect(bounds_,UITheme::PanelBackground,10.f);
    renderer.StrokeRect(bounds_,UITheme::PanelBorder,1.f);
    renderer.Text(title,{bounds_.X+18.f,bounds_.Y+9.f,bounds_.W-34.f,37.f},
                  kText,UIFont::Heading);
    renderer.Line(bounds_.X+13.f,bounds_.Y+44.f,bounds_.X+bounds_.W-13.f,
                  bounds_.Y+44.f,UITheme::Divider);
}
