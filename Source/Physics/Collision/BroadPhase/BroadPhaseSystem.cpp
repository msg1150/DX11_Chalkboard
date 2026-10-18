// [칠판 프로젝트에서의 역할]
// X축 시작 위치로 정렬한 Sweep-and-Prune 방식입니다.
// X 구간이 멀어지면 탐색을 끝내고 Layer/Mask와 나머지 축 겹침을 검사합니다.

// ============================================================================
// BroadPhaseSystem.cpp
// ============================================================================
#include "Physics/Collision/BroadPhase/BroadPhaseSystem.h"

#include <algorithm>

namespace Collision
{
    std::vector<BroadPhaseSystem::Pair> BroadPhaseSystem::BuildPairs(
        const std::vector<ColliderComponent>& colliders)const
    {
        std::vector<BroadPhaseProxy> proxies;
        proxies.reserve(colliders.size());

        for(std::size_t i=0;i<colliders.size();++i)
        {
            const auto& collider=colliders[i];
            if(!collider.Enabled)continue;
            proxies.push_back({i,ComputeWorldBounds(collider)});
        }

        // X축 최소값 순으로 정렬한 뒤 현재 Proxy의 MaxX를 넘는 순간 Scan을 멈춘다.
        // 작은 Editor/Test Scene에서는 구현이 단순하면서도 O(N^2) 전수검사보다
        // 불필요한 Narrow Phase 호출을 크게 줄일 수 있다.
        std::sort(proxies.begin(),proxies.end(),[](const BroadPhaseProxy& a,const BroadPhaseProxy& b)
        {
            return a.Bounds.Min.x<b.Bounds.Min.x;
        });

        std::vector<Pair> pairs;
        for(std::size_t i=0;i<proxies.size();++i)
        {
            const auto& aProxy=proxies[i];
            const auto& a=colliders[aProxy.Index];

            for(std::size_t j=i+1;j<proxies.size();++j)
            {
                const auto& bProxy=proxies[j];
                if(bProxy.Bounds.Min.x>aProxy.Bounds.Max.x)break;

                const auto& b=colliders[bProxy.Index];

                // 양쪽 Mask가 모두 상대 Layer를 허용해야 실제 Collision Pair가 된다.
                if((a.Mask&b.Layer)==0u || (b.Mask&a.Layer)==0u)continue;
                if(!Overlaps(aProxy.Bounds,bProxy.Bounds))continue;

                pairs.emplace_back(aProxy.Index,bProxy.Index);
            }
        }
        return pairs;
    }
}
