// [칠판 프로젝트에서의 역할]
// 충돌 전용 Vec3·기저·Bounds와 순수 수학 함수입니다. DirectX 렌더링과 분리되어 있습니다.

// ============================================================================
// CollisionMath.h
// ----------------------------------------------------------------------------
// Collision / Query 코드가 DirectXMath, Win32, D3D11에 직접 의존하지 않도록
// 사용하는 최소 3D 수학 자료형과 보조 함수를 정의한다.
//
// 이 파일은 Portable Test에서도 그대로 사용된다. 따라서 Renderer나 GPU 관련
// 타입을 포함하지 않는다. DirectX::XMFLOAT3 변환은 Feature 계층에서 수행한다.
// ============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Collision
{
    constexpr float kEpsilon = 1.0e-5f;
    constexpr float kPi = 3.14159265358979323846f;

    struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        constexpr Vec3() = default;
        constexpr Vec3(float px, float py, float pz) : x(px), y(py), z(pz) {}

        constexpr Vec3 operator+(const Vec3& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
        constexpr Vec3 operator-(const Vec3& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
        constexpr Vec3 operator-() const { return {-x, -y, -z}; }
        constexpr Vec3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
        constexpr Vec3 operator/(float scalar) const { return {x / scalar, y / scalar, z / scalar}; }
        Vec3& operator+=(const Vec3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
        Vec3& operator-=(const Vec3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    };

    inline Vec3 operator*(float scalar, const Vec3& value) { return value * scalar; }
    inline float Dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
    inline Vec3 Cross(const Vec3& a, const Vec3& b)
    {
        return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
    }
    inline float LengthSq(const Vec3& v) { return Dot(v,v); }
    inline float Length(const Vec3& v) { return std::sqrt(LengthSq(v)); }
    // 길이가 너무 작을 때 나눗셈 대신 기본 방향을 반환해 NaN/불안정한 충돌 법선을 피합니다.
    inline Vec3 Normalize(const Vec3& v, const Vec3& fallback={1.f,0.f,0.f})
    {
        const float len=Length(v);
        return len>kEpsilon ? v/len : fallback;
    }
    inline Vec3 Abs(const Vec3& v){return {std::abs(v.x),std::abs(v.y),std::abs(v.z)};}
    inline Vec3 Min(const Vec3& a,const Vec3& b){return {std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)};}
    inline Vec3 Max(const Vec3& a,const Vec3& b){return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)};}
    inline Vec3 Clamp(const Vec3& v,const Vec3& lo,const Vec3& hi)
    {
        return {std::clamp(v.x,lo.x,hi.x),std::clamp(v.y,lo.y,hi.y),std::clamp(v.z,lo.z,hi.z)};
    }
    inline float Clamp01(float v){return std::clamp(v,0.f,1.f);}
    inline float DegreesToRadians(float degrees){return degrees*(kPi/180.f);}
    inline float RadiansToDegrees(float radians){return radians*(180.f/kPi);}

    // X -> Y -> Z 순서로 Local Vector에 Euler 회전을 적용한다.
    // Collision Debug와 실제 충돌 계산이 같은 함수를 사용하므로 Shape가 시각화와
    // 판정에서 서로 다른 방향을 바라보는 문제를 막는다.
    inline Vec3 RotateEulerXYZ(Vec3 v,const Vec3& rotation)
    {
        const float cx=std::cos(rotation.x),sx=std::sin(rotation.x);
        const float cy=std::cos(rotation.y),sy=std::sin(rotation.y);
        const float cz=std::cos(rotation.z),sz=std::sin(rotation.z);

        v={v.x, v.y*cx-v.z*sx, v.y*sx+v.z*cx};
        v={v.x*cy+v.z*sy, v.y, -v.x*sy+v.z*cy};
        v={v.x*cz-v.y*sz, v.x*sz+v.y*cz, v.z};
        return v;
    }

    struct Basis3
    {
        Vec3 X{1.f,0.f,0.f};
        Vec3 Y{0.f,1.f,0.f};
        Vec3 Z{0.f,0.f,1.f};
    };

    // Euler 회전을 로컬 X/Y/Z 단위축에 적용하여 회전된 직교 기저를 구성합니다.
    inline Basis3 BuildBasis(const Vec3& rotation)
    {
        Basis3 basis;
        basis.X=Normalize(RotateEulerXYZ({1.f,0.f,0.f},rotation));
        basis.Y=Normalize(RotateEulerXYZ({0.f,1.f,0.f},rotation));
        basis.Z=Normalize(RotateEulerXYZ({0.f,0.f,1.f},rotation));
        return basis;
    }

    // 로컬 좌표를 각 기저 방향으로 조합하고 중심 위치를 더해 월드 좌표를 구합니다.
    inline Vec3 LocalToWorld(const Vec3& local,const Vec3& center,const Basis3& basis)
    {
        return center + basis.X*local.x + basis.Y*local.y + basis.Z*local.z;
    }
    // 월드 상대 위치를 정규화된 기저에 내적하여 로컬 좌표로 바꿉니다.
    inline Vec3 WorldToLocal(const Vec3& world,const Vec3& center,const Basis3& basis)
    {
        const Vec3 delta=world-center;
        return {Dot(delta,basis.X),Dot(delta,basis.Y),Dot(delta,basis.Z)};
    }

    struct AABB
    {
        Vec3 Min{};
        Vec3 Max{};

        Vec3 Center()const{return (Min+Max)*0.5f;}
        Vec3 HalfExtents()const{return (Max-Min)*0.5f;}
        bool IsValid()const{return Min.x<=Max.x && Min.y<=Max.y && Min.z<=Max.z;}
    };

    // 세 축의 Min/Max 구간이 모두 겹치는지 검사합니다. 경계 접촉도 겹침에 포함합니다.
    inline bool Overlaps(const AABB& a,const AABB& b)
    {
        return a.Min.x<=b.Max.x && a.Max.x>=b.Min.x &&
               a.Min.y<=b.Max.y && a.Max.y>=b.Min.y &&
               a.Min.z<=b.Max.z && a.Max.z>=b.Min.z;
    }

    // 점을 선분에 투영한 비율 t를 0~1로 제한하여 유한 선분 위의 최근접 점을 구합니다.
    inline Vec3 ClosestPointOnSegment(const Vec3& p,const Vec3& a,const Vec3& b,float* outT=nullptr)
    {
        const Vec3 ab=b-a;
        const float denom=LengthSq(ab);
        const float t=denom>kEpsilon ? Clamp01(Dot(p-a,ab)/denom) : 0.f;
        if(outT)*outT=t;
        return a+ab*t;
    }
}
