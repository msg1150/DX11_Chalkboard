// [칠판 프로젝트에서의 역할]
// 로컬 Shape 설정과 Collider의 변환·Layer/Mask를 정의합니다.
// ToWorld 계열은 좁은 단계 검사에 쓸 형상을, ComputeWorldBounds는 넓은 단계 AABB를 만듭니다.

// ============================================================================
// CollisionTypes.h
// ----------------------------------------------------------------------------
// Collision Component가 보관하는 Shape 데이터와 World 변환 결과를 정의한다.
// Shape 자체는 게임 오브젝트/Renderer를 모르며 순수 데이터로만 유지한다.
// ============================================================================
#pragma once

#include "Physics/Collision/Core/CollisionMath.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

namespace Collision
{
    using ColliderId = std::uint32_t;
    using LayerMask = std::uint32_t;

    constexpr LayerMask LayerDefault = 1u << 0u;
    constexpr LayerMask LayerTerrain = 1u << 1u;
    constexpr LayerMask LayerTrigger = 1u << 2u;
    constexpr LayerMask LayerEditor = 1u << 3u;
    constexpr LayerMask LayerAll = 0xFFFFFFFFu;

    enum class ShapeType : std::uint8_t
    {
        Sphere,
        AABB,
        OBB,
        Capsule
    };

    struct SphereShape
    {
        float Radius = 1.0f;
    };

    struct AABBShape
    {
        Vec3 HalfExtents{1.0f,1.0f,1.0f};
    };

    struct OBBShape
    {
        Vec3 HalfExtents{1.0f,1.0f,1.0f};
    };

    // HalfSegment는 Capsule 중앙에서 위/아래 구의 중심까지의 거리이다.
    // 따라서 Capsule 전체 반높이는 HalfSegment + Radius이다.
    struct CapsuleShape
    {
        float Radius = 1.0f;
        float HalfSegment = 1.5f;
    };

    using ShapeData = std::variant<SphereShape,AABBShape,OBBShape,CapsuleShape>;

    struct ColliderComponent
    {
        ColliderId Id = 0;
        std::wstring Name;
        Vec3 Position{};

        // Sphere/AABB는 회전이 판정에 영향을 주지 않는다.
        // OBB/Capsule만 이 Euler 값(X/Y/Z Radian)을 사용한다.
        Vec3 Rotation{};
        ShapeData Shape = SphereShape{};

        // Layer는 자신이 속한 Bit, Mask는 충돌을 허용할 상대 Layer Bit 집합이다.
        LayerMask Layer = LayerDefault;
        LayerMask Mask = LayerAll;

        bool Enabled = true;
        bool IsTrigger = false;
    };

    struct WorldSphere
    {
        Vec3 Center{};
        float Radius=1.f;
    };
    struct WorldOBB
    {
        Vec3 Center{};
        Vec3 HalfExtents{1.f,1.f,1.f};
        Basis3 Basis{};
    };
    struct WorldCapsule
    {
        Vec3 A{};
        Vec3 B{};
        float Radius=1.f;
    };

    inline ShapeType GetShapeType(const ShapeData& shape)
    {
        return static_cast<ShapeType>(shape.index());
    }
    inline ShapeType GetShapeType(const ColliderComponent& collider)
    {
        return GetShapeType(collider.Shape);
    }

    inline const wchar_t* ShapeTypeName(ShapeType type)
    {
        switch(type)
        {
        case ShapeType::Sphere:return L"Sphere";
        case ShapeType::AABB:return L"AABB";
        case ShapeType::OBB:return L"OBB";
        case ShapeType::Capsule:return L"Capsule";
        }
        return L"Unknown";
    }

    inline WorldSphere ToWorldSphere(const ColliderComponent& c,const SphereShape& shape)
    {
        return {c.Position,std::max(0.01f,shape.Radius)};
    }
    inline AABB ToWorldAABB(const ColliderComponent& c,const AABBShape& shape)
    {
        const Vec3 e=Max(Abs(shape.HalfExtents),{0.01f,0.01f,0.01f});
        return {c.Position-e,c.Position+e};
    }
    inline WorldOBB ToWorldOBB(const ColliderComponent& c,const OBBShape& shape)
    {
        WorldOBB result;
        result.Center=c.Position;
        result.HalfExtents=Max(Abs(shape.HalfExtents),{0.01f,0.01f,0.01f});
        result.Basis=BuildBasis(c.Rotation);
        return result;
    }
    inline WorldCapsule ToWorldCapsule(const ColliderComponent& c,const CapsuleShape& shape)
    {
        const float radius=std::max(0.01f,shape.Radius);
        const float halfSegment=std::max(0.f,shape.HalfSegment);
        const Basis3 basis=BuildBasis(c.Rotation);
        return {c.Position-basis.Y*halfSegment,c.Position+basis.Y*halfSegment,radius};
    }

    inline WorldOBB AABBAsOBB(const AABB& box)
    {
        return {box.Center(),box.HalfExtents(),{}};
    }

    // Broad Phase용 World AABB. 어떤 Shape라도 이 Bounds를 제공하므로
    // Narrow Phase를 호출하기 전에 저렴하게 후보 Pair를 줄일 수 있다.
    inline AABB ComputeWorldBounds(const ColliderComponent& c)
    {
        return std::visit([&](const auto& shape)->AABB
        {
            using T=std::decay_t<decltype(shape)>;
            if constexpr(std::is_same_v<T,SphereShape>)
            {
                const auto s=ToWorldSphere(c,shape);
                const Vec3 e{s.Radius,s.Radius,s.Radius};
                return {s.Center-e,s.Center+e};
            }
            else if constexpr(std::is_same_v<T,AABBShape>)
            {
                return ToWorldAABB(c,shape);
            }
            else if constexpr(std::is_same_v<T,OBBShape>)
            {
                const auto box=ToWorldOBB(c,shape);
                const Vec3 e=
                    Abs(box.Basis.X)*box.HalfExtents.x+
                    Abs(box.Basis.Y)*box.HalfExtents.y+
                    Abs(box.Basis.Z)*box.HalfExtents.z;
                return {box.Center-e,box.Center+e};
            }
            else
            {
                const auto capsule=ToWorldCapsule(c,shape);
                const Vec3 e{capsule.Radius,capsule.Radius,capsule.Radius};
                return {Min(capsule.A,capsule.B)-e,Max(capsule.A,capsule.B)+e};
            }
        },c.Shape);
    }
}
