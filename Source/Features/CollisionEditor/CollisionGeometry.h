// [칠판 프로젝트에서의 역할]
// 충돌 Shape를 시각화용 정점으로 바꾸는 어댑터입니다.
// 칠판 외형의 박스 생성, Collider 선 표시, GPU Picking용 Solid Proxy에 재사용합니다.

// ============================================================================
// CollisionGeometry.h
// ----------------------------------------------------------------------------
// Collision Shape의 수학적 결과를 D3D11 Debug/Picking용 Vertex로 변환한다.
// Collision Core는 Graphics를 모르고, 이 Adapter만 Graphics::Vertex를 사용한다.
// ============================================================================
#pragma once

#include "Graphics/Mesh.h"
#include "Physics/Collision/Core/CollisionTypes.h"

#include <DirectXMath.h>
#include <vector>

namespace CollisionGeometry
{
    void AppendWireframe(const Collision::ColliderComponent& collider,
                         const DirectX::XMFLOAT4& color,
                         std::vector<Vertex>& outVertices);

    void BuildSolidTriangles(const Collision::ColliderComponent& collider,
                             std::vector<Vertex>& outVertices);
}
