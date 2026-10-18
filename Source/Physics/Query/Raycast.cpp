// [칠판 프로젝트에서의 역할]
// Ray를 정규화하고 Shape별 교차식을 적용합니다. 회전 박스는 Ray를 로컬 공간으로 옮겨 계산합니다.

// ============================================================================
// Raycast.cpp
// ============================================================================
#include "Physics/Query/Raycast.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace Collision
{
namespace
{
    RaycastHit Miss(){return {};}
    // 거리 t와 정규화된 Ray 방향으로 월드 교점과 법선을 채웁니다.
    RaycastHit MakeHit(ColliderId id,float t,const Vec3& origin,const Vec3& direction,const Vec3& normal)
    {
        RaycastHit hit;
        hit.Hit=true;hit.Collider=id;hit.Distance=t;
        hit.Point=origin+direction*t;hit.Normal=Normalize(normal,{0.f,1.f,0.f});
        return hit;
    }

    // Ray-구 교차의 이차방정식을 풀어 가까운 해를 선택합니다. 구 내부에서 시작하면 거리를 0으로 처리합니다.
    RaycastHit RaySphere(const Ray& ray,const ColliderComponent& c,const SphereShape& shape,float maxDistance)
    {
        const Vec3 d=Normalize(ray.Direction,{0.f,0.f,1.f});
        const auto sphere=ToWorldSphere(c,shape);
        const Vec3 m=ray.Origin-sphere.Center;
        const float b=Dot(m,d);
        const float cc=Dot(m,m)-sphere.Radius*sphere.Radius;
        if(cc>0.f&&b>0.f)return Miss();
        const float discr=b*b-cc;
        if(discr<0.f)return Miss();
        float t=-b-std::sqrt(discr);
        if(t<0.f)t=0.f;
        if(t>maxDistance)return Miss();
        const Vec3 p=ray.Origin+d*t;
        return MakeHit(c.Id,t,ray.Origin,d,p-sphere.Center);
    }

    // 박스 X/Y/Z 구간에 진입/이탈하는 t 범위의 교집합을 구합니다. 공통 구간이 없으면 빗나간 Ray입니다.
    bool Slab(const Vec3& origin,const Vec3& direction,const Vec3& e,float maxDistance,
              float& outT,Vec3& outNormal)
    {
        float tMin=0.f,tMax=maxDistance;
        Vec3 normal{};
        const float o[3]={origin.x,origin.y,origin.z};
        const float d[3]={direction.x,direction.y,direction.z};
        const float ex[3]={e.x,e.y,e.z};
        for(int axis=0;axis<3;++axis)
        {
            if(std::abs(d[axis])<kEpsilon)
            {
                if(o[axis]<-ex[axis]||o[axis]>ex[axis])return false;
                continue;
            }
            float t1=(-ex[axis]-o[axis])/d[axis];
            float t2=( ex[axis]-o[axis])/d[axis];
            float sign=-1.f;
            if(t1>t2){std::swap(t1,t2);sign=1.f;}
            if(t1>tMin)
            {
                tMin=t1;
                normal={0.f,0.f,0.f};
                if(axis==0)normal.x=sign;
                if(axis==1)normal.y=sign;
                if(axis==2)normal.z=sign;
            }
            tMax=std::min(tMax,t2);
            if(tMin>tMax)return false;
        }
        outT=tMin;
        outNormal=normal;
        return tMin<=maxDistance;
    }

    // 원점/방향을 OBB의 로컬 공간으로 옮겨 Slab을 적용한 뒤 법선을 월드 방향으로 복원합니다.
    RaycastHit RayOBB(const Ray& ray,const ColliderComponent& c,const WorldOBB& box,float maxDistance)
    {
        const Vec3 worldD=Normalize(ray.Direction,{0.f,0.f,1.f});
        const Vec3 o=WorldToLocal(ray.Origin,box.Center,box.Basis);
        const Vec3 d={Dot(worldD,box.Basis.X),Dot(worldD,box.Basis.Y),Dot(worldD,box.Basis.Z)};
        float t=0.f;Vec3 localNormal{};
        if(!Slab(o,d,box.HalfExtents,maxDistance,t,localNormal))return Miss();
        const Vec3 worldNormal=box.Basis.X*localNormal.x+box.Basis.Y*localNormal.y+box.Basis.Z*localNormal.z;
        return MakeHit(c.Id,t,ray.Origin,worldD,worldNormal);
    }

    // 중앙 원통 부분과 양 끝 구를 검사해 가장 가까운 교점을 선택합니다.
    RaycastHit RayCapsule(const Ray& ray,const ColliderComponent& c,const CapsuleShape& shape,float maxDistance)
    {
        const Vec3 rd=Normalize(ray.Direction,{0.f,0.f,1.f});
        const auto cap=ToWorldCapsule(c,shape);
        const Vec3 ba=cap.B-cap.A;
        const Vec3 oa=ray.Origin-cap.A;
        const float baba=Dot(ba,ba);

        // Capsule 축 길이가 0이면 Sphere와 동일하다.
        if(baba<=kEpsilon)
        {
            ColliderComponent temp=c;
            temp.Position=cap.A;
            return RaySphere(ray,temp,SphereShape{cap.Radius},maxDistance);
        }

        const float bard=Dot(ba,rd);
        const float baoa=Dot(ba,oa);
        const float rdoa=Dot(rd,oa);
        const float oaoa=Dot(oa,oa);
        const float a=baba-bard*bard;
        const float b=baba*rdoa-baoa*bard;
        const float cc=baba*oaoa-baoa*baoa-cap.Radius*cap.Radius*baba;
        float best=std::numeric_limits<float>::max();
        Vec3 bestNormal{};

        if(std::abs(a)>kEpsilon)
        {
            const float h=b*b-a*cc;
            if(h>=0.f)
            {
                const float t=(-b-std::sqrt(h))/a;
                const float y=baoa+t*bard;
                if(t>=0.f&&t<=maxDistance&&y>0.f&&y<baba)
                {
                    const Vec3 p=ray.Origin+rd*t;
                    const Vec3 axisPoint=cap.A+ba*(y/baba);
                    best=t;bestNormal=p-axisPoint;
                }
            }
        }

        auto testSphere=[&](const Vec3& center)
        {
            const Vec3 m=ray.Origin-center;
            const float qb=Dot(m,rd);
            const float qc=Dot(m,m)-cap.Radius*cap.Radius;
            const float h=qb*qb-qc;
            if(h<0.f)return;
            float t=-qb-std::sqrt(h);
            if(t<0.f)t=-qb+std::sqrt(h);
            if(t>=0.f&&t<best&&t<=maxDistance)
            {
                best=t;bestNormal=(ray.Origin+rd*t)-center;
            }
        };
        testSphere(cap.A);testSphere(cap.B);
        if(best==std::numeric_limits<float>::max())return Miss();
        return MakeHit(c.Id,best,ray.Origin,rd,bestNormal);
    }

    RaycastHit TestShape(const Ray& ray,const ColliderComponent& c,const SphereShape& s,float maxDistance){return RaySphere(ray,c,s,maxDistance);}
    RaycastHit TestShape(const Ray& ray,const ColliderComponent& c,const AABBShape& s,float maxDistance){return RayOBB(ray,c,AABBAsOBB(ToWorldAABB(c,s)),maxDistance);}
    RaycastHit TestShape(const Ray& ray,const ColliderComponent& c,const OBBShape& s,float maxDistance){return RayOBB(ray,c,ToWorldOBB(c,s),maxDistance);}
    RaycastHit TestShape(const Ray& ray,const ColliderComponent& c,const CapsuleShape& s,float maxDistance){return RayCapsule(ray,c,s,maxDistance);}
}

// 비활성 Collider나 음수 조회 거리를 거부하고 Shape variant에 맞는 교차 함수를 호출합니다.
RaycastHit Raycast::Test(const Ray& ray,const ColliderComponent& collider,float maxDistance)
{
    if(!collider.Enabled||maxDistance<0.f)return {};
    return std::visit([&](const auto& shape){return TestShape(ray,collider,shape,maxDistance);},collider.Shape);
}

// Layer 조건을 통과한 Collider 중 가장 가까운 Hit를 유지합니다. 다음 검사 거리를 현재 최단 거리로 줄입니다.
RaycastHit Raycast::Closest(const Ray& ray,const std::vector<ColliderComponent>& colliders,
                            LayerMask mask,float maxDistance)
{
    RaycastHit best{};
    best.Distance=maxDistance;
    for(const auto& collider:colliders)
    {
        if(!collider.Enabled||(collider.Layer&mask)==0u)continue;
        const auto hit=Test(ray,collider,best.Distance);
        if(hit.Hit&&hit.Distance<=best.Distance)best=hit;
    }
    return best;
}
}
