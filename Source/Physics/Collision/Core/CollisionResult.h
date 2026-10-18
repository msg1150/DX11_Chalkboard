// [칠판 프로젝트에서의 역할]
// 충돌 여부·접촉 위치·A→B 법선·침투 깊이·Trigger 여부를 담는 결과입니다.

// ============================================================================
// CollisionResult.h
// ----------------------------------------------------------------------------
// Detection은 "충돌했는가"뿐 아니라 이후 Response/Debug가 재사용할
// 접촉점, 법선, 침투 깊이를 반환한다.
// ============================================================================
#pragma once

#include "Physics/Collision/Core/CollisionTypes.h"

namespace Collision
{
    struct CollisionResult
    {
        bool Hit=false;
        ColliderId ColliderA=0;
        ColliderId ColliderB=0;
        Vec3 Point{};

        // 가능한 경우 A -> B 방향을 가리킨다.
        Vec3 Normal{0.f,1.f,0.f};
        float PenetrationDepth=0.f;
        bool TriggerPair=false;
    };
}
