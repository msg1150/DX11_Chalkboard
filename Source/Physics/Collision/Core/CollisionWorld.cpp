// [칠판 프로젝트에서의 역할]
// Collider ID는 삭제 후 재사용하지 않습니다. 늦게 도착한 GPU 결과가 다른 물체를 가리키는 것을 막습니다.
// 이 시스템은 충돌 감지/조회까지 담당하며 물리 반응으로 위치를 밀어내지는 않습니다.

// ============================================================================
// CollisionWorld.cpp
// ============================================================================
#include "Physics/Collision/Core/CollisionWorld.h"
#include "Physics/Collision/Detection/CollisionDetector.h"

#include <algorithm>
#include <utility>

namespace Collision
{
    // Shape 공통 생성 경로입니다. 증가하는 ID와 이름을 지정한 뒤 Collider 배열에 소유권을 넣습니다.
    ColliderId CollisionWorld::Add(ShapeData shape,const Vec3& position,const wchar_t* baseName)
    {
        ColliderComponent collider;
        collider.Id=nextId_++;
        collider.Name=std::wstring(baseName)+L" #"+std::to_wstring(collider.Id);
        collider.Position=position;
        collider.Shape=std::move(shape);
        colliders_.push_back(std::move(collider));
        return colliders_.back().Id;
    }

    // 구의 반지름을 양수로 제한하고 공통 등록 함수를 호출합니다.
    ColliderId CollisionWorld::AddSphere(const Vec3& p,float radius)
    {
        return Add(SphereShape{std::max(0.01f,radius)},p,L"Sphere");
    }
    // 각 축 반크기를 양수로 제한하여 회전 없는 박스를 등록합니다.
    ColliderId CollisionWorld::AddAABB(const Vec3& p,const Vec3& e)
    {
        return Add(AABBShape{Max(Abs(e),{0.01f,0.01f,0.01f})},p,L"AABB");
    }
    // 회전 가능한 박스를 등록합니다. 칠판 본체와 받침도 이 경로를 사용합니다.
    ColliderId CollisionWorld::AddOBB(const Vec3& p,const Vec3& e)
    {
        return Add(OBBShape{Max(Abs(e),{0.01f,0.01f,0.01f})},p,L"OBB");
    }
    // 반지름은 양수, 중심 선분 반길이는 0 이상이 되도록 정리합니다.
    ColliderId CollisionWorld::AddCapsule(const Vec3& p,float radius,float halfSegment)
    {
        return Add(CapsuleShape{std::max(0.01f,radius),std::max(0.f,halfSegment)},p,L"Capsule");
    }

    // Collider 삭제와 함께 해당 ID의 충돌 강조/접촉 결과도 지워 오래된 참조를 남기지 않습니다.
    bool CollisionWorld::Remove(ColliderId id)
    {
        const auto it=std::find_if(colliders_.begin(),colliders_.end(),[id](const ColliderComponent& c){return c.Id==id;});
        if(it==colliders_.end())return false;
        colliders_.erase(it);
        collidingIds_.erase(id);
        contacts_.erase(std::remove_if(contacts_.begin(),contacts_.end(),[id](const CollisionResult& r)
        {return r.ColliderA==id||r.ColliderB==id;}),contacts_.end());
        return true;
    }

    // 모든 Collider와 충돌 결과를 비우되 nextId_는 되돌리지 않습니다.
    void CollisionWorld::Clear()
    {
        colliders_.clear();
        contacts_.clear();
        collidingIds_.clear();
        // ID는 재사용하지 않는다. GPU Readback이 한두 Frame 늦게 도착해도
        // 삭제된 ID가 새 Collider를 잘못 선택하는 상황을 막기 위함이다.
    }

    // ID로 편집 가능한 Collider를 찾습니다. 없으면 nullptr을 반환합니다.
    ColliderComponent* CollisionWorld::Find(ColliderId id)
    {
        const auto it=std::find_if(colliders_.begin(),colliders_.end(),[id](const ColliderComponent& c){return c.Id==id;});
        return it==colliders_.end()?nullptr:&*it;
    }
    // 읽기 전용 조회입니다. 비동기 선택 결과에 해당 ID가 아직 존재하는지 확인할 때도 사용할 수 있습니다.
    const ColliderComponent* CollisionWorld::Find(ColliderId id)const
    {
        const auto it=std::find_if(colliders_.begin(),colliders_.end(),[id](const ColliderComponent& c){return c.Id==id;});
        return it==colliders_.end()?nullptr:&*it;
    }

    // 이전 결과를 비우고 Broad Phase 후보만 Narrow Phase에 전달하여 실제 접촉과 충돌 ID 집합을 만듭니다.
    void CollisionWorld::UpdateCollisions()
    {
        contacts_.clear();
        collidingIds_.clear();
        const auto pairs=broadPhase_.BuildPairs(colliders_);
        contacts_.reserve(pairs.size());
        for(const auto& [aIndex,bIndex]:pairs)
        {
            CollisionResult result=CollisionDetector::Test(colliders_[aIndex],colliders_[bIndex]);
            if(!result.Hit)continue;
            collidingIds_.insert(result.ColliderA);
            collidingIds_.insert(result.ColliderB);
            contacts_.push_back(result);
        }
    }

    // 최근 UpdateCollisions에서 충돌한 Collider인지 ID 집합으로 조회합니다.
    bool CollisionWorld::IsColliding(ColliderId id)const
    {
        return collidingIds_.find(id)!=collidingIds_.end();
    }

    // 지정 영역과 Collider의 월드 Bounds가 겹치는 ID를 조회합니다. 실제 Shape 간 상세 판정과는 다른 Bounds 조회입니다.
    std::vector<ColliderId> CollisionWorld::OverlapAABB(const AABB& bounds,LayerMask mask)const
    {
        std::vector<ColliderId> result;
        for(const auto& collider:colliders_)
        {
            if(!collider.Enabled||(collider.Layer&mask)==0u)continue;
            if(Overlaps(bounds,ComputeWorldBounds(collider)))result.push_back(collider.Id);
        }
        return result;
    }
}
