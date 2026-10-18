// [칠판 프로젝트에서의 역할]
// Collider 등록·삭제·검색과 충돌 결과 수집을 담당합니다. 칠판 본체와 받침을 OBB로 등록합니다.

// ============================================================================
// CollisionWorld.h
// ----------------------------------------------------------------------------
// Collider의 소유/ID/충돌 상태를 관리하는 Runtime World.
// Physics Response나 Rigidbody는 넣지 않고 Detection/Query까지만 담당한다.
// ============================================================================
#pragma once

#include "Physics/Collision/BroadPhase/BroadPhaseSystem.h"
#include "Physics/Collision/Core/CollisionResult.h"

#include <unordered_set>
#include <vector>

namespace Collision
{
    class CollisionWorld
    {
    public:
        ColliderId AddSphere(const Vec3& position,float radius=2.f);
        ColliderId AddAABB(const Vec3& position,const Vec3& halfExtents={2.f,2.f,2.f});
        ColliderId AddOBB(const Vec3& position,const Vec3& halfExtents={2.f,2.f,2.f});
        ColliderId AddCapsule(const Vec3& position,float radius=1.5f,float halfSegment=2.f);

        bool Remove(ColliderId id);
        void Clear();

        ColliderComponent* Find(ColliderId id);
        const ColliderComponent* Find(ColliderId id)const;

        void UpdateCollisions();

        bool IsColliding(ColliderId id)const;
        const std::vector<CollisionResult>& Contacts()const{return contacts_;}
        const std::vector<ColliderComponent>& Colliders()const{return colliders_;}
        std::vector<ColliderComponent>& Colliders(){return colliders_;}
        std::size_t Count()const{return colliders_.size();}

        // Broad Phase Bounds와 겹치는 Collider ID를 반환하는 간단한 Scene Query.
        std::vector<ColliderId> OverlapAABB(const AABB& bounds,LayerMask mask=LayerAll)const;

    private:
        ColliderId Add(ShapeData shape,const Vec3& position,const wchar_t* baseName);

        std::vector<ColliderComponent> colliders_;
        std::vector<CollisionResult> contacts_;
        std::unordered_set<ColliderId> collidingIds_;
        BroadPhaseSystem broadPhase_;
        ColliderId nextId_=1;
    };
}
