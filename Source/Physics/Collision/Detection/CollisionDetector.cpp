// [칠판 프로젝트에서의 역할]
// 구·박스·캡슐의 실제 형상을 검사하는 Narrow Phase입니다.
// 박스 쌍은 SAT를, 둥근 형상은 최근접 점/선분 거리와 반지름을 이용합니다.

// ============================================================================
// CollisionDetector.cpp
// ----------------------------------------------------------------------------
// Sphere / AABB / OBB / Capsule의 모든 조합을 Narrow Phase로 판정한다.
// 판정 함수는 Shape Pair별로 독립되어 있어 새로운 Shape를 추가할 때
// 기존 ColliderComponent / BroadPhase를 수정하지 않아도 된다.
// ============================================================================
#include "Physics/Collision/Detection/CollisionDetector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <type_traits>
#include <vector>

namespace Collision
{
namespace
{
    // 비충돌 결과에도 두 Collider ID와 Trigger 여부를 남겨 결과의 대상을 식별합니다.
    CollisionResult Miss(const ColliderComponent& a,const ColliderComponent& b)
    {
        CollisionResult result;
        result.ColliderA=a.Id;
        result.ColliderB=b.Id;
        result.TriggerPair=a.IsTrigger||b.IsTrigger;
        return result;
    }

    // 공통 충돌 결과를 만듭니다. 법선은 정규화하고 침투 깊이는 음수가 되지 않게 제한합니다.
    CollisionResult Hit(const ColliderComponent& a,const ColliderComponent& b,
                        const Vec3& point,const Vec3& normal,float penetration)
    {
        CollisionResult result=Miss(a,b);
        result.Hit=true;
        result.Point=point;
        result.Normal=Normalize(normal,{0.f,1.f,0.f});
        result.PenetrationDepth=std::max(0.f,penetration);
        return result;
    }

    // 점의 상대 위치를 박스의 회전된 세 축에 투영한 뒤 반크기 범위로 제한하여 최근접 점을 구합니다.
    Vec3 OBBClosestPoint(const WorldOBB& box,const Vec3& point)
    {
        const Vec3 d=point-box.Center;
        Vec3 result=box.Center;
        const std::array<Vec3,3> axes={box.Basis.X,box.Basis.Y,box.Basis.Z};
        const std::array<float,3> ext={box.HalfExtents.x,box.HalfExtents.y,box.HalfExtents.z};
        for(std::size_t i=0;i<3;++i)
        {
            const float distance=std::clamp(Dot(d,axes[i]),-ext[i],ext[i]);
            result+=axes[i]*distance;
        }
        return result;
    }

    // 지정 방향으로 가장 멀리 있는 박스 꼭짓점을 구해 접촉 위치의 대표값 계산에 사용합니다.
    Vec3 OBBSupport(const WorldOBB& box,const Vec3& direction)
    {
        Vec3 result=box.Center;
        result+=box.Basis.X*(Dot(box.Basis.X,direction)>=0.f?box.HalfExtents.x:-box.HalfExtents.x);
        result+=box.Basis.Y*(Dot(box.Basis.Y,direction)>=0.f?box.HalfExtents.y:-box.HalfExtents.y);
        result+=box.Basis.Z*(Dot(box.Basis.Z,direction)>=0.f?box.HalfExtents.z:-box.HalfExtents.z);
        return result;
    }

    // Point가 Box 내부에 있을 때 가장 가까운 면과 그 면까지의 거리를 구한다.
    // Capsule 축이 Box 내부를 지나 Distance=0이 되는 경우 Normal을 안정적으로
    // 만들기 위해 사용한다.
    void NearestLocalFace(const Vec3& p,const Vec3& e,Vec3& outPoint,Vec3& outNormal,float& outDistance)
    {
        const float dx=e.x-std::abs(p.x);
        const float dy=e.y-std::abs(p.y);
        const float dz=e.z-std::abs(p.z);
        outPoint=p;
        if(dx<=dy && dx<=dz)
        {
            const float sign=p.x>=0.f?1.f:-1.f;
            outPoint.x=sign*e.x;
            outNormal={sign,0.f,0.f};
            outDistance=std::max(0.f,dx);
        }
        else if(dy<=dz)
        {
            const float sign=p.y>=0.f?1.f:-1.f;
            outPoint.y=sign*e.y;
            outNormal={0.f,sign,0.f};
            outDistance=std::max(0.f,dy);
        }
        else
        {
            const float sign=p.z>=0.f?1.f:-1.f;
            outPoint.z=sign*e.z;
            outNormal={0.f,0.f,sign};
            outDistance=std::max(0.f,dz);
        }
    }

    // Segment와 AABB 사이의 정확한 최소 거리를 Piecewise Quadratic으로 계산한다.
    // Segment의 각 좌표가 Min/Max 경계를 지나는 t를 Breakpoint로 모으면 각 구간에서
    // Point-AABB 거리 제곱은 단순 2차식이 된다. 구간별 미분값의 0점과 양 끝만 평가한다.
    float ClosestSegmentAABB(const Vec3& p0,const Vec3& p1,const Vec3& e,
                             Vec3& outSegmentPoint,Vec3& outBoxPoint)
    {
        const Vec3 d=p1-p0;
        std::vector<float> breaks={0.f,1.f};
        const float p[3]={p0.x,p0.y,p0.z};
        const float dv[3]={d.x,d.y,d.z};
        const float ex[3]={e.x,e.y,e.z};
        for(int axis=0;axis<3;++axis)
        {
            if(std::abs(dv[axis])<=kEpsilon)continue;
            const float t0=(-ex[axis]-p[axis])/dv[axis];
            const float t1=( ex[axis]-p[axis])/dv[axis];
            if(t0>0.f&&t0<1.f)breaks.push_back(t0);
            if(t1>0.f&&t1<1.f)breaks.push_back(t1);
        }
        std::sort(breaks.begin(),breaks.end());
        breaks.erase(std::unique(breaks.begin(),breaks.end(),[](float a,float b)
        {return std::abs(a-b)<1.0e-6f;}),breaks.end());

        float best=std::numeric_limits<float>::max();
        float bestT=0.f;
        auto evaluate=[&](float t)
        {
            const Vec3 s=p0+d*t;
            const Vec3 q=Clamp(s,{-e.x,-e.y,-e.z},e);
            const float dist=LengthSq(s-q);
            if(dist<best)
            {
                best=dist;
                bestT=t;
                outSegmentPoint=s;
                outBoxPoint=q;
            }
        };

        for(std::size_t i=0;i+1<breaks.size();++i)
        {
            const float left=breaks[i],right=breaks[i+1];
            const float mid=(left+right)*0.5f;
            const Vec3 m=p0+d*mid;

            float numerator=0.f,denominator=0.f;
            const float mv[3]={m.x,m.y,m.z};
            for(int axis=0;axis<3;++axis)
            {
                float bound=0.f;
                bool active=false;
                if(mv[axis]<-ex[axis]){bound=-ex[axis];active=true;}
                else if(mv[axis]>ex[axis]){bound=ex[axis];active=true;}
                if(active)
                {
                    // d/dt (p0 + d*t - bound)^2 = 0
                    numerator+=dv[axis]*(p[axis]-bound);
                    denominator+=dv[axis]*dv[axis];
                }
            }

            evaluate(left);
            evaluate(right);
            if(denominator>kEpsilon)
            {
                const float stationary=std::clamp(-numerator/denominator,left,right);
                evaluate(stationary);
            }
        }

        // Segment가 Point처럼 축약된 경우에도 값이 채워지도록 한다.
        if(breaks.size()==1u)evaluate(0.f);
        (void)bestT;
        return best;
    }

    // 두 유한 선분의 최근접 점을 구합니다. 길이 0 또는 평행인 경우와 끝점 범위를 별도로 처리합니다.
    void ClosestSegmentSegment(const Vec3& p1,const Vec3& q1,
                               const Vec3& p2,const Vec3& q2,
                               Vec3& outA,Vec3& outB)
    {
        const Vec3 d1=q1-p1;
        const Vec3 d2=q2-p2;
        const Vec3 r=p1-p2;
        const float a=Dot(d1,d1);
        const float e=Dot(d2,d2);
        const float f=Dot(d2,r);
        float s=0.f,t=0.f;

        if(a<=kEpsilon && e<=kEpsilon)
        {
            outA=p1;outB=p2;return;
        }
        if(a<=kEpsilon)
        {
            t=Clamp01(f/e);
        }
        else
        {
            const float c=Dot(d1,r);
            if(e<=kEpsilon)
            {
                s=Clamp01(-c/a);
            }
            else
            {
                const float b=Dot(d1,d2);
                const float denom=a*e-b*b;
                if(std::abs(denom)>kEpsilon)s=Clamp01((b*f-c*e)/denom);
                t=(b*s+f)/e;
                if(t<0.f){t=0.f;s=Clamp01(-c/a);}
                else if(t>1.f){t=1.f;s=Clamp01((b-c)/a);}
            }
        }
        outA=p1+d1*s;
        outB=p2+d2*t;
    }

    // 중심 거리와 두 반지름의 합을 비교합니다. 중심이 같을 때도 유효한 기본 법선을 사용합니다.
    CollisionResult SphereSphere(const ColliderComponent& a,const SphereShape& sa,
                                 const ColliderComponent& b,const SphereShape& sb)
    {
        const auto A=ToWorldSphere(a,sa),B=ToWorldSphere(b,sb);
        const Vec3 delta=B.Center-A.Center;
        const float distance=Length(delta);
        const float sum=A.Radius+B.Radius;
        if(distance>sum)return Miss(a,b);
        const Vec3 normal=distance>kEpsilon?delta/distance:Vec3{1.f,0.f,0.f};
        const Vec3 point=A.Center+normal*(A.Radius-(sum-distance)*0.5f);
        return Hit(a,b,point,normal,sum-distance);
    }

    // 구 중심에서 박스까지의 최근접 거리로 판정합니다. 중심이 내부이면 가장 가까운 면으로 법선을 정합니다.
    CollisionResult SphereOBBImpl(const ColliderComponent& a,const WorldSphere& sphere,
                                  const ColliderComponent& b,const WorldOBB& box)
    {
        const Vec3 closest=OBBClosestPoint(box,sphere.Center);
        Vec3 delta=closest-sphere.Center; // Sphere(A) -> Box(B)
        float distance=Length(delta);
        if(distance>sphere.Radius)return Miss(a,b);

        if(distance<=kEpsilon)
        {
            const Vec3 local=WorldToLocal(sphere.Center,box.Center,box.Basis);
            Vec3 localFace{},localNormal{};float faceDistance=0.f;
            NearestLocalFace(local,box.HalfExtents,localFace,localNormal,faceDistance);
            const Vec3 worldFace=LocalToWorld(localFace,box.Center,box.Basis);
            const Vec3 worldNormal=Normalize(
                box.Basis.X*localNormal.x+box.Basis.Y*localNormal.y+box.Basis.Z*localNormal.z);
            return Hit(a,b,worldFace,worldNormal,sphere.Radius+faceDistance);
        }
        const Vec3 normal=delta/distance;
        return Hit(a,b,closest,normal,sphere.Radius-distance);
    }

    // 회전 없는 AABB를 OBB 형태로 해석하여 구-박스 공통 계산을 재사용합니다.
    CollisionResult SphereAABB(const ColliderComponent& a,const SphereShape& sa,
                               const ColliderComponent& b,const AABBShape& sb)
    {
        return SphereOBBImpl(a,ToWorldSphere(a,sa),b,AABBAsOBB(ToWorldAABB(b,sb)));
    }
    // Collider의 위치와 회전으로 월드 구/OBB를 만든 뒤 공통 구-박스 계산에 전달합니다.
    CollisionResult SphereOBB(const ColliderComponent& a,const SphereShape& sa,
                              const ColliderComponent& b,const OBBShape& sb)
    {
        return SphereOBBImpl(a,ToWorldSphere(a,sa),b,ToWorldOBB(b,sb));
    }

    // 구 중심과 캡슐 중심 선분 사이 최근접 거리를 두 반지름의 합과 비교합니다.
    CollisionResult SphereCapsule(const ColliderComponent& a,const SphereShape& sa,
                                  const ColliderComponent& b,const CapsuleShape& sb)
    {
        const auto sphere=ToWorldSphere(a,sa);
        const auto capsule=ToWorldCapsule(b,sb);
        const Vec3 axisPoint=ClosestPointOnSegment(sphere.Center,capsule.A,capsule.B);
        const Vec3 delta=axisPoint-sphere.Center;
        const float distance=Length(delta);
        const float sum=sphere.Radius+capsule.Radius;
        if(distance>sum)return Miss(a,b);
        Vec3 normal=distance>kEpsilon?delta/distance:Normalize(capsule.B-capsule.A,{0.f,1.f,0.f});
        const Vec3 point=sphere.Center+normal*(sphere.Radius-(sum-distance)*0.5f);
        return Hit(a,b,point,normal,sum-distance);
    }

    // 세 축의 겹침을 검사하고 가장 적게 겹친 축을 대표 충돌 법선/침투 깊이로 사용합니다.
    CollisionResult AABBAABB(const ColliderComponent& a,const AABBShape& sa,
                             const ColliderComponent& b,const AABBShape& sb)
    {
        const auto A=ToWorldAABB(a,sa),B=ToWorldAABB(b,sb);
        if(!Overlaps(A,B))return Miss(a,b);

        const float ox=std::min(A.Max.x,B.Max.x)-std::max(A.Min.x,B.Min.x);
        const float oy=std::min(A.Max.y,B.Max.y)-std::max(A.Min.y,B.Min.y);
        const float oz=std::min(A.Max.z,B.Max.z)-std::max(A.Min.z,B.Min.z);
        Vec3 normal{};float penetration=ox;
        const Vec3 centers=B.Center()-A.Center();
        normal={centers.x>=0.f?1.f:-1.f,0.f,0.f};
        if(oy<penetration){penetration=oy;normal={0.f,centers.y>=0.f?1.f:-1.f,0.f};}
        if(oz<penetration){penetration=oz;normal={0.f,0.f,centers.z>=0.f?1.f:-1.f};}
        const Vec3 point=(Max(A.Min,B.Min)+Min(A.Max,B.Max))*0.5f;
        return Hit(a,b,point,normal,penetration);
    }

    // SAT: 각 박스의 면 법선 6개와 축끼리의 외적 9개를 검사합니다.
    // 한 축이라도 투영 구간이 분리되면 비충돌이며 모두 겹치면 최소 겹침 축을 결과에 사용합니다.
    CollisionResult OBBPair(const ColliderComponent& a,const WorldOBB& A,
                            const ColliderComponent& b,const WorldOBB& B)
    {
        const std::array<Vec3,3> axesA={A.Basis.X,A.Basis.Y,A.Basis.Z};
        const std::array<Vec3,3> axesB={B.Basis.X,B.Basis.Y,B.Basis.Z};
        const float eA[3]={A.HalfExtents.x,A.HalfExtents.y,A.HalfExtents.z};
        const float eB[3]={B.HalfExtents.x,B.HalfExtents.y,B.HalfExtents.z};
        const Vec3 centerDelta=B.Center-A.Center;

        float minOverlap=std::numeric_limits<float>::max();
        Vec3 minAxis{1.f,0.f,0.f};

        auto testAxis=[&](Vec3 axis)->bool
        {
            const float lenSq=LengthSq(axis);
            if(lenSq<=1.0e-8f)return true; // 거의 평행한 축의 Cross는 SAT 축에서 제외.
            axis=axis/std::sqrt(lenSq);
            const float ra=eA[0]*std::abs(Dot(axesA[0],axis))+
                           eA[1]*std::abs(Dot(axesA[1],axis))+
                           eA[2]*std::abs(Dot(axesA[2],axis));
            const float rb=eB[0]*std::abs(Dot(axesB[0],axis))+
                           eB[1]*std::abs(Dot(axesB[1],axis))+
                           eB[2]*std::abs(Dot(axesB[2],axis));
            const float signedDistance=Dot(centerDelta,axis);
            const float overlap=ra+rb-std::abs(signedDistance);
            if(overlap<0.f)return false;
            if(overlap<minOverlap)
            {
                minOverlap=overlap;
                minAxis=signedDistance>=0.f?axis:-axis;
            }
            return true;
        };

        for(const auto& axis:axesA)if(!testAxis(axis))return Miss(a,b);
        for(const auto& axis:axesB)if(!testAxis(axis))return Miss(a,b);
        for(const auto& aa:axesA)
            for(const auto& bb:axesB)
                if(!testAxis(Cross(aa,bb)))return Miss(a,b);

        const Vec3 supportA=OBBSupport(A,minAxis);
        const Vec3 supportB=OBBSupport(B,-minAxis);
        return Hit(a,b,(supportA+supportB)*0.5f,minAxis,minOverlap);
    }

    // AABB를 회전 없는 OBB로 바꿔 박스-박스 SAT를 재사용합니다.
    CollisionResult AABBOBB(const ColliderComponent& a,const AABBShape& sa,
                            const ColliderComponent& b,const OBBShape& sb)
    {
        return OBBPair(a,AABBAsOBB(ToWorldAABB(a,sa)),b,ToWorldOBB(b,sb));
    }
    // 두 Collider의 월드 OBB를 만든 후 SAT에 전달합니다.
    CollisionResult OBBOBB(const ColliderComponent& a,const OBBShape& sa,
                           const ColliderComponent& b,const OBBShape& sb)
    {
        return OBBPair(a,ToWorldOBB(a,sa),b,ToWorldOBB(b,sb));
    }

    // 캡슐 축을 박스 로컬 좌표로 옮겨 선분-AABB 최근접 거리를 구합니다.
    // 반지름보다 가까우면 충돌이며 축이 박스 내부를 통과할 때는 가장 가까운 면을 사용합니다.
    CollisionResult CapsuleOBBImpl(const ColliderComponent& a,const WorldCapsule& capsule,
                                   const ColliderComponent& b,const WorldOBB& box)
    {
        const Vec3 localA=WorldToLocal(capsule.A,box.Center,box.Basis);
        const Vec3 localB=WorldToLocal(capsule.B,box.Center,box.Basis);
        Vec3 localSeg{},localBox{};
        const float distanceSq=ClosestSegmentAABB(localA,localB,box.HalfExtents,localSeg,localBox);
        const float distance=std::sqrt(std::max(0.f,distanceSq));
        if(distance>capsule.Radius)return Miss(a,b);

        if(distance>kEpsilon)
        {
            const Vec3 worldSeg=LocalToWorld(localSeg,box.Center,box.Basis);
            const Vec3 worldBox=LocalToWorld(localBox,box.Center,box.Basis);
            const Vec3 normal=Normalize(worldBox-worldSeg);
            return Hit(a,b,worldBox,normal,capsule.Radius-distance);
        }

        Vec3 facePoint{},faceNormal{};float faceDistance=0.f;
        NearestLocalFace(localSeg,box.HalfExtents,facePoint,faceNormal,faceDistance);
        const Vec3 worldFace=LocalToWorld(facePoint,box.Center,box.Basis);
        const Vec3 worldNormal=Normalize(
            box.Basis.X*faceNormal.x+box.Basis.Y*faceNormal.y+box.Basis.Z*faceNormal.z);
        return Hit(a,b,worldFace,worldNormal,capsule.Radius+faceDistance);
    }

    // AABB를 회전 없는 OBB로 해석해 캡슐-박스 공통 계산을 재사용합니다.
    CollisionResult CapsuleAABB(const ColliderComponent& a,const CapsuleShape& sa,
                                const ColliderComponent& b,const AABBShape& sb)
    {
        return CapsuleOBBImpl(a,ToWorldCapsule(a,sa),b,AABBAsOBB(ToWorldAABB(b,sb)));
    }
    // 캡슐과 박스의 변환을 월드 형상에 적용한 뒤 공통 계산을 호출합니다.
    CollisionResult CapsuleOBB(const ColliderComponent& a,const CapsuleShape& sa,
                               const ColliderComponent& b,const OBBShape& sb)
    {
        return CapsuleOBBImpl(a,ToWorldCapsule(a,sa),b,ToWorldOBB(b,sb));
    }

    // 두 중심 선분의 최근접 점 간 거리를 두 반지름의 합과 비교합니다.
    CollisionResult CapsuleCapsule(const ColliderComponent& a,const CapsuleShape& sa,
                                   const ColliderComponent& b,const CapsuleShape& sb)
    {
        const auto A=ToWorldCapsule(a,sa),B=ToWorldCapsule(b,sb);
        Vec3 pa{},pb{};
        ClosestSegmentSegment(A.A,A.B,B.A,B.B,pa,pb);
        const Vec3 delta=pb-pa;
        const float distance=Length(delta);
        const float sum=A.Radius+B.Radius;
        if(distance>sum)return Miss(a,b);
        const Vec3 normal=distance>kEpsilon?delta/distance:Normalize(B.B-A.B,{1.f,0.f,0.f});
        const Vec3 point=pa+normal*(A.Radius-(sum-distance)*0.5f);
        return Hit(a,b,point,normal,sum-distance);
    }

    // Shape 인자를 바꿔 재사용한 판정 결과의 ID와 법선을 원래 A→B 방향으로 되돌립니다.
    CollisionResult Flip(CollisionResult r)
    {
        std::swap(r.ColliderA,r.ColliderB);
        r.Normal=-r.Normal;
        return r;
    }

    // std::visit가 Shape Pair의 실제 타입을 컴파일 타임에 선택한다.
    CollisionResult Pair(const ColliderComponent& a,const SphereShape& sa,const ColliderComponent& b,const SphereShape& sb){return SphereSphere(a,sa,b,sb);}
    CollisionResult Pair(const ColliderComponent& a,const SphereShape& sa,const ColliderComponent& b,const AABBShape& sb){return SphereAABB(a,sa,b,sb);}
    CollisionResult Pair(const ColliderComponent& a,const SphereShape& sa,const ColliderComponent& b,const OBBShape& sb){return SphereOBB(a,sa,b,sb);}
    CollisionResult Pair(const ColliderComponent& a,const SphereShape& sa,const ColliderComponent& b,const CapsuleShape& sb){return SphereCapsule(a,sa,b,sb);}

    CollisionResult Pair(const ColliderComponent& a,const AABBShape& sa,const ColliderComponent& b,const SphereShape& sb){return Flip(SphereAABB(b,sb,a,sa));}
    CollisionResult Pair(const ColliderComponent& a,const AABBShape& sa,const ColliderComponent& b,const AABBShape& sb){return AABBAABB(a,sa,b,sb);}
    CollisionResult Pair(const ColliderComponent& a,const AABBShape& sa,const ColliderComponent& b,const OBBShape& sb){return AABBOBB(a,sa,b,sb);}
    CollisionResult Pair(const ColliderComponent& a,const AABBShape& sa,const ColliderComponent& b,const CapsuleShape& sb){return Flip(CapsuleAABB(b,sb,a,sa));}

    CollisionResult Pair(const ColliderComponent& a,const OBBShape& sa,const ColliderComponent& b,const SphereShape& sb){return Flip(SphereOBB(b,sb,a,sa));}
    CollisionResult Pair(const ColliderComponent& a,const OBBShape& sa,const ColliderComponent& b,const AABBShape& sb){return Flip(AABBOBB(b,sb,a,sa));}
    CollisionResult Pair(const ColliderComponent& a,const OBBShape& sa,const ColliderComponent& b,const OBBShape& sb){return OBBOBB(a,sa,b,sb);}
    CollisionResult Pair(const ColliderComponent& a,const OBBShape& sa,const ColliderComponent& b,const CapsuleShape& sb){return Flip(CapsuleOBB(b,sb,a,sa));}

    CollisionResult Pair(const ColliderComponent& a,const CapsuleShape& sa,const ColliderComponent& b,const SphereShape& sb){return Flip(SphereCapsule(b,sb,a,sa));}
    CollisionResult Pair(const ColliderComponent& a,const CapsuleShape& sa,const ColliderComponent& b,const AABBShape& sb){return CapsuleAABB(a,sa,b,sb);}
    CollisionResult Pair(const ColliderComponent& a,const CapsuleShape& sa,const ColliderComponent& b,const OBBShape& sb){return CapsuleOBB(a,sa,b,sb);}
    CollisionResult Pair(const ColliderComponent& a,const CapsuleShape& sa,const ColliderComponent& b,const CapsuleShape& sb){return CapsuleCapsule(a,sa,b,sb);}
}

// 비활성/Layer-Mask 조건을 확인하고 std::visit로 실제 Shape 타입 조합에 맞는 Pair를 호출합니다.
CollisionResult CollisionDetector::Test(const ColliderComponent& a,const ColliderComponent& b)
{
    if(!a.Enabled||!b.Enabled)return Miss(a,b);
    if((a.Mask&b.Layer)==0u||(b.Mask&a.Layer)==0u)return Miss(a,b);
    return std::visit([&](const auto& sa,const auto& sb)
    {
        return Pair(a,sa,b,sb);
    },a.Shape,b.Shape);
}
}
