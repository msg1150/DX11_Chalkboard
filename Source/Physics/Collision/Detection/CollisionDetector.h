// [칠판 프로젝트에서의 역할]
// Shape Pair별 상세 충돌 검사의 공용 진입점입니다.

// ============================================================================
// CollisionDetector.h
// ----------------------------------------------------------------------------
// Shape Pair별 Narrow Phase 알고리즘의 단일 진입점.
// std::variant Visitor를 사용하므로 GameObject/Renderer 계층과 분리된다.
// ============================================================================
#pragma once

#include "Physics/Collision/Core/CollisionResult.h"

namespace Collision
{
    class CollisionDetector
    {
    public:
        static CollisionResult Test(const ColliderComponent& a,const ColliderComponent& b);
    };
}
