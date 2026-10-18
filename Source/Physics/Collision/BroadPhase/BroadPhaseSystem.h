// [칠판 프로젝트에서의 역할]
// 저렴한 World AABB 검사로 상세 충돌 검사를 할 후보 쌍을 추립니다.

// ============================================================================
// BroadPhaseSystem.h
// ----------------------------------------------------------------------------
// 모든 Collider Pair를 바로 Narrow Phase에 보내지 않고 World AABB 기반
// Sweep-and-Prune으로 후보를 줄인다. 나중에 Dynamic AABB Tree 등으로
// 교체하더라도 CollisionWorld / Detector는 바뀌지 않도록 별도 모듈이다.
// ============================================================================
#pragma once

#include "Physics/Collision/Core/CollisionTypes.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace Collision
{
    struct BroadPhaseProxy
    {
        std::size_t Index=0;
        AABB Bounds{};
    };

    class BroadPhaseSystem
    {
    public:
        using Pair=std::pair<std::size_t,std::size_t>;
        std::vector<Pair> BuildPairs(const std::vector<ColliderComponent>& colliders)const;
    };
}
