// [칠판 프로젝트에서의 역할]
// Label/CheckBox/Slider/Button/Panel의 인터페이스와 바인딩을 정의합니다.
// 칠판의 RGB와 크기는 Slider의 Getter/Setter로 연결합니다.

// ============================================================================
// UIWidget.h : 게임 로직을 모르는 재사용 가능한 Retained Widget 계층.
// ============================================================================
#pragma once
#include "UI/UIRenderer.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
class UIWidget
{
public:
    virtual ~UIWidget()=default;
    virtual float PreferredHeight()const=0;
    virtual void Render(UIRenderer& renderer,bool hovered)const=0;
    virtual bool HitTest(float x,float y)const{return bounds_.Contains(x,y);}
    virtual void PointerDown(float x,float y){(void)x;(void)y;}
    virtual void PointerDrag(float x,float y){(void)x;(void)y;}
    virtual void PointerUp(float x,float y){(void)x;(void)y;}
    // 선택적으로 숫자 입력에 반응하는 Widget만 override한다.
    virtual bool IsTextEditing()const{return false;}
    virtual void HandleTextCharacter(wchar_t ch){(void)ch;}
    virtual void CommitTextEdit(){}
    virtual void CancelTextEdit(){}

    void SetBounds(UIRect rect){bounds_=rect;}
    const UIRect& Bounds()const{return bounds_;}
protected:
    UIRect bounds_={};
};
class UILabel final : public UIWidget
{
public:
    explicit UILabel(std::wstring text):text_(std::move(text)){}
    explicit UILabel(std::function<std::wstring()> getter):getter_(std::move(getter)){}
    float PreferredHeight()const override{return 22.f;}
    bool HitTest(float,float)const override{return false;}
    void Render(UIRenderer& renderer,bool)const override;
private:
    std::wstring text_;
    std::function<std::wstring()> getter_;
};
class UICheckBox final : public UIWidget
{
public:
    UICheckBox(std::wstring label,std::function<bool()> getter,
               std::function<void(bool)> setter);
    float PreferredHeight()const override{return 28.f;}
    void Render(UIRenderer& renderer,bool hovered)const override;
    void PointerUp(float x,float y)override;
private:
    std::wstring label_;
    std::function<bool()> getter_;
    std::function<void(bool)> setter_;
};
class UISlider final : public UIWidget
{
public:
    UISlider(std::wstring label,float minimum,float maximum,float step,
             std::function<float()> getter,std::function<void(float)> setter);
    float PreferredHeight()const override{return 66.f;}
    void Render(UIRenderer& renderer,bool hovered)const override;
    void PointerDown(float x,float y)override;
    void PointerDrag(float x,float y)override;
    bool IsTextEditing()const override{return editing_;}
    void HandleTextCharacter(wchar_t ch)override;
    void CommitTextEdit()override;
    void CancelTextEdit()override;
private:
    void SetFromMouse(float x);
    UIRect NumberRect()const;
    void BeginTextEdit();
    std::wstring label_;
    float min_,max_,step_;
    std::function<float()> getter_;
    std::function<void(float)> setter_;
    bool editing_=false;
    bool selectAll_=false; // 처음 입력한 문자는 원래 값을 전체 교체한다.
    std::wstring editText_;
};
// 일반 버튼: 게임 기능을 몰라도 Callback으로 동작한다.
class UIButton final : public UIWidget
{
public:
    UIButton(std::wstring label,std::function<void()> onClick)
        :label_(std::move(label)),onClick_(std::move(onClick)){}
    void SetLabel(std::wstring label){label_=std::move(label);}
    float PreferredHeight()const override{return 34.f;}
    void Render(UIRenderer& renderer,bool hovered)const override;
    void PointerUp(float x,float y)override;
private:
    std::wstring label_;
    std::function<void()> onClick_;
};
class UIPanel
{
public:
    void SetBounds(UIRect rect){bounds_=rect;}
    const UIRect& Bounds()const{return bounds_;}
    void Render(UIRenderer& renderer,const std::wstring& title)const;
private:
    UIRect bounds_={};
};
