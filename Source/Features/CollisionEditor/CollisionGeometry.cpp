// [칠판 프로젝트에서의 역할]
// Sphere/Box/Capsule의 선과 삼각형을 만듭니다. 실제 충돌 수학은 Physics에 있습니다.
// 와이어프레임 출력은 기존 배열에 추가하고 Solid 출력은 Shape 하나의 삼각형으로 다시 구성합니다.

// ============================================================================
// CollisionGeometry.cpp
// ============================================================================
#include "Features/CollisionEditor/CollisionGeometry.h"

#include <array>
#include <cmath>
#include <type_traits>

using namespace Collision;
using namespace DirectX;

namespace
{
    XMFLOAT3 ToFloat3(const Vec3& v){return {v.x,v.y,v.z};}

    // 충돌 수학의 Vec3를 공통 렌더링 Vertex로 변환합니다. 기본 UV/색도 함께 초기화합니다.
    Vertex MakeVertex(const Vec3& p,const XMFLOAT4& color={1.f,1.f,1.f,1.f})
    {
        Vertex v{};
        v.Position=ToFloat3(p);
        v.Normal={0.f,1.f,0.f};
        v.UV={0.f,0.f};
        v.Color=color;
        return v;
    }

    void Line(std::vector<Vertex>& out,const Vec3& a,const Vec3& b,const XMFLOAT4& color)
    {
        out.push_back(MakeVertex(a,color));
        out.push_back(MakeVertex(b,color));
    }

    // 세 점으로 삼각형 법선을 계산하여 Solid Proxy나 칠판 박스에 사용할 정점 3개를 추가합니다.
    void Triangle(std::vector<Vertex>& out,const Vec3& a,const Vec3& b,const Vec3& c)
    {
        const Vec3 n=Normalize(Cross(b-a,c-a),{0.f,1.f,0.f});
        Vertex va=MakeVertex(a),vb=MakeVertex(b),vc=MakeVertex(c);
        va.Normal=vb.Normal=vc.Normal=ToFloat3(n);
        out.push_back(va);out.push_back(vb);out.push_back(vc);
    }

    std::array<Vec3,8> BoxCorners(const WorldOBB& box)
    {
        const Vec3 ex=box.Basis.X*box.HalfExtents.x;
        const Vec3 ey=box.Basis.Y*box.HalfExtents.y;
        const Vec3 ez=box.Basis.Z*box.HalfExtents.z;
        return {
            box.Center-ex-ey-ez, box.Center+ex-ey-ez,
            box.Center+ex+ey-ez, box.Center-ex+ey-ez,
            box.Center-ex-ey+ez, box.Center+ex-ey+ez,
            box.Center+ex+ey+ez, box.Center-ex+ey+ez
        };
    }

    // OBB 꼭짓점 8개를 12개 모서리로 연결하여 선 표시를 만듭니다.
    void BoxLines(const WorldOBB& box,const XMFLOAT4& color,std::vector<Vertex>& out)
    {
        const auto c=BoxCorners(box);
        constexpr int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for(const auto& e:edges)Line(out,c[e[0]],c[e[1]],color);
    }

    // OBB의 여섯 면을 각 2개, 총 12개 삼각형으로 만듭니다.
    void BoxTriangles(const WorldOBB& box,std::vector<Vertex>& out)
    {
        const auto c=BoxCorners(box);
        constexpr int tris[12][3]={
            {0,2,1},{0,3,2}, {4,5,6},{4,6,7},
            {0,1,5},{0,5,4}, {3,7,6},{3,6,2},
            {1,2,6},{1,6,5}, {0,4,7},{0,7,3}
        };
        for(const auto& t:tris)Triangle(out,c[t[0]],c[t[1]],c[t[2]]);
    }

    // 서로 수직인 큰 원 3개로 구의 윤곽을 표현합니다.
    void SphereLines(const WorldSphere& sphere,const XMFLOAT4& color,std::vector<Vertex>& out)
    {
        constexpr int segments=32;
        for(int plane=0;plane<3;++plane)
        {
            for(int i=0;i<segments;++i)
            {
                const float a0=2.f*kPi*float(i)/float(segments);
                const float a1=2.f*kPi*float(i+1)/float(segments);
                Vec3 p0{},p1{};
                if(plane==0){p0={std::cos(a0),std::sin(a0),0};p1={std::cos(a1),std::sin(a1),0};}
                else if(plane==1){p0={std::cos(a0),0,std::sin(a0)};p1={std::cos(a1),0,std::sin(a1)};}
                else{p0={0,std::cos(a0),std::sin(a0)};p1={0,std::cos(a1),std::sin(a1)};}
                Line(out,sphere.Center+p0*sphere.Radius,sphere.Center+p1*sphere.Radius,color);
            }
        }
    }

    // 위도/경도 격자를 삼각형으로 연결하여 구의 Solid Proxy를 생성합니다.
    void SphereTriangles(const WorldSphere& sphere,std::vector<Vertex>& out)
    {
        constexpr int stacks=12;
        constexpr int slices=20;
        for(int y=0;y<stacks;++y)
        {
            const float v0=float(y)/float(stacks);
            const float v1=float(y+1)/float(stacks);
            const float phi0=-0.5f*kPi+v0*kPi;
            const float phi1=-0.5f*kPi+v1*kPi;
            for(int x=0;x<slices;++x)
            {
                const float u0=2.f*kPi*float(x)/float(slices);
                const float u1=2.f*kPi*float(x+1)/float(slices);
                auto point=[&](float phi,float theta)
                {
                    const float cp=std::cos(phi);
                    return sphere.Center+Vec3{cp*std::cos(theta),std::sin(phi),cp*std::sin(theta)}*sphere.Radius;
                };
                const Vec3 a=point(phi0,u0),b=point(phi0,u1),c=point(phi1,u1),d=point(phi1,u0);
                if(y!=0)Triangle(out,a,c,b);
                if(y!=stacks-1)Triangle(out,a,d,c);
            }
        }
    }

    // 캡슐 축에 수직인 두 방향을 만들어 원통 둘레와 반구의 기준 좌표계를 구성합니다.
    void CapsuleBasis(const WorldCapsule& capsule,Vec3& axis,Vec3& u,Vec3& v)
    {
        axis=Normalize(capsule.B-capsule.A,{0.f,1.f,0.f});
        const Vec3 helper=std::abs(axis.y)<0.9f?Vec3{0.f,1.f,0.f}:Vec3{1.f,0.f,0.f};
        u=Normalize(Cross(helper,axis),{1.f,0.f,0.f});
        v=Normalize(Cross(axis,u),{0.f,0.f,1.f});
    }

    // 캡슐의 중심 원통과 끝부분을 선으로 표현합니다.
    void CapsuleLines(const WorldCapsule& cap,const XMFLOAT4& color,std::vector<Vertex>& out)
    {
        Vec3 axis{},u{},v{};CapsuleBasis(cap,axis,u,v);
        constexpr int segments=24;
        // 양 끝 Cylinder Ring.
        for(int i=0;i<segments;++i)
        {
            const float a0=2.f*kPi*float(i)/float(segments),a1=2.f*kPi*float(i+1)/float(segments);
            const Vec3 r0=(u*std::cos(a0)+v*std::sin(a0))*cap.Radius;
            const Vec3 r1=(u*std::cos(a1)+v*std::sin(a1))*cap.Radius;
            Line(out,cap.A+r0,cap.A+r1,color);
            Line(out,cap.B+r0,cap.B+r1,color);
        }
        // Cylinder Side 4개 + Hemisphere Arc 4방향.
        constexpr int directions=4,arcSegments=10;
        for(int d=0;d<directions;++d)
        {
            const float theta=0.5f*kPi*float(d);
            const Vec3 radial=u*std::cos(theta)+v*std::sin(theta);
            Line(out,cap.A+radial*cap.Radius,cap.B+radial*cap.Radius,color);
            for(int i=0;i<arcSegments;++i)
            {
                const float t0=0.5f*kPi*float(i)/float(arcSegments);
                const float t1=0.5f*kPi*float(i+1)/float(arcSegments);
                Line(out,cap.B+radial*(std::cos(t0)*cap.Radius)+axis*(std::sin(t0)*cap.Radius),
                         cap.B+radial*(std::cos(t1)*cap.Radius)+axis*(std::sin(t1)*cap.Radius),color);
                Line(out,cap.A+radial*(std::cos(t0)*cap.Radius)-axis*(std::sin(t0)*cap.Radius),
                         cap.A+radial*(std::cos(t1)*cap.Radius)-axis*(std::sin(t1)*cap.Radius),color);
            }
        }
    }

    // 캡슐의 중앙 원통과 양 끝 반구를 삼각형으로 구성합니다.
    void CapsuleTriangles(const WorldCapsule& cap,std::vector<Vertex>& out)
    {
        Vec3 axis{},u{},v{};CapsuleBasis(cap,axis,u,v);
        constexpr int slices=18;
        constexpr int hemi=6;

        struct Ring{Vec3 center;float radius;};
        std::vector<Ring> rings;
        rings.reserve(2*hemi+4);
        // Bottom pole -> Bottom equator.
        rings.push_back({cap.A-axis*cap.Radius,0.f});
        for(int j=1;j<=hemi;++j)
        {
            const float t=float(j)/float(hemi);
            const float angle=-0.5f*kPi+t*0.5f*kPi;
            rings.push_back({cap.A+axis*(std::sin(angle)*cap.Radius),std::cos(angle)*cap.Radius});
        }
        // Top equator를 별도 Ring으로 넣어 Cylinder Side를 만든다.
        rings.push_back({cap.B,cap.Radius});
        for(int j=1;j<hemi;++j)
        {
            const float t=float(j)/float(hemi);
            const float angle=t*0.5f*kPi;
            rings.push_back({cap.B+axis*(std::sin(angle)*cap.Radius),std::cos(angle)*cap.Radius});
        }
        rings.push_back({cap.B+axis*cap.Radius,0.f});

        auto ringPoint=[&](const Ring& ring,int slice)
        {
            const float theta=2.f*kPi*float(slice%slices)/float(slices);
            return ring.center+(u*std::cos(theta)+v*std::sin(theta))*ring.radius;
        };
        for(std::size_t r=0;r+1<rings.size();++r)
        {
            for(int s=0;s<slices;++s)
            {
                const Vec3 a=ringPoint(rings[r],s),b=ringPoint(rings[r],s+1);
                const Vec3 c=ringPoint(rings[r+1],s+1),d=ringPoint(rings[r+1],s);
                if(rings[r].radius>kEpsilon)Triangle(out,a,c,b);
                if(rings[r+1].radius>kEpsilon)Triangle(out,a,d,c);
            }
        }
    }
}

namespace CollisionGeometry
{
// Shape variant에 맞는 와이어프레임을 기존 정점 배열 뒤에 추가합니다. 여러 Collider 선을 한 번에 모을 수 있습니다.
void AppendWireframe(const ColliderComponent& collider,const XMFLOAT4& color,std::vector<Vertex>& out)
{
    std::visit([&](const auto& shape)
    {
        using T=std::decay_t<decltype(shape)>;
        if constexpr(std::is_same_v<T,SphereShape>)SphereLines(ToWorldSphere(collider,shape),color,out);
        else if constexpr(std::is_same_v<T,AABBShape>)BoxLines(AABBAsOBB(ToWorldAABB(collider,shape)),color,out);
        else if constexpr(std::is_same_v<T,OBBShape>)BoxLines(ToWorldOBB(collider,shape),color,out);
        else CapsuleLines(ToWorldCapsule(collider,shape),color,out);
    },collider.Shape);
}

// 출력 배열을 비운 후 Collider 하나의 Solid 삼각형을 만듭니다. Picking은 이 배열을 동적 버퍼에 업로드합니다.
void BuildSolidTriangles(const ColliderComponent& collider,std::vector<Vertex>& out)
{
    out.clear();
    std::visit([&](const auto& shape)
    {
        using T=std::decay_t<decltype(shape)>;
        if constexpr(std::is_same_v<T,SphereShape>)SphereTriangles(ToWorldSphere(collider,shape),out);
        else if constexpr(std::is_same_v<T,AABBShape>)BoxTriangles(AABBAsOBB(ToWorldAABB(collider,shape)),out);
        else if constexpr(std::is_same_v<T,OBBShape>)BoxTriangles(ToWorldOBB(collider,shape),out);
        else CapsuleTriangles(ToWorldCapsule(collider,shape),out);
    },collider.Shape);
}
}
