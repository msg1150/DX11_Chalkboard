// [칠판 프로젝트에서의 역할]
// Yaw/Pitch/거리로 Orbit 위치를 계산하고 DirectX 왼손 좌표계의 행렬을 만듭니다.
// 카메라 투영 행렬은 렌더링과 마우스 Ray 생성에 공통으로 사용합니다.

// ============================================================================
// Camera.cpp
// ============================================================================

#include "Graphics/Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

void Camera::SetPerspective(
    float fieldOfViewRadians,
    float aspectRatio,
    float nearPlane,
    float farPlane)
{
    fieldOfView_ = fieldOfViewRadians;
    aspectRatio_ = aspectRatio;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
}

void Camera::SetAspectRatio(
    float aspectRatio)
{
    aspectRatio_ = aspectRatio;
}

void Camera::SetTarget(
    const XMFLOAT3& target)
{
    target_ = target;
}

const XMFLOAT3& Camera::GetTarget() const
{
    return target_;
}

void Camera::MoveTarget(
    float deltaX,
    float deltaY,
    float deltaZ)
{
    target_.x += deltaX;
    target_.y += deltaY;
    target_.z += deltaZ;
}

void Camera::SetOrbit(
    float yawRadians,
    float pitchRadians,
    float distance)
{
    yaw_ = yawRadians;
    pitch_ = pitchRadians;
    distance_ = distance;

    // Pitch가 90도를 넘어가면 Up Vector와 바라보는 방향이
    // 거의 평행해져 View Matrix가 뒤집힐 수 있다.
    // 따라서 -80 ~ +80도로 제한한다.
    pitch_ =
        std::clamp(
            pitch_,
            XMConvertToRadians(-80.0f),
            XMConvertToRadians(80.0f));

    // 지나치게 가까워져 Object 내부로 들어가거나
    // 너무 멀리 벗어나지 않도록 기본 거리 범위를 둔다.
    distance_ =
        std::clamp(
            distance_,
            2.0f,
            200.0f);
}

void Camera::AddOrbit(
    float yawDelta,
    float pitchDelta)
{
    yaw_ += yawDelta;
    pitch_ += pitchDelta;

    pitch_ =
        std::clamp(
            pitch_,
            XMConvertToRadians(-80.0f),
            XMConvertToRadians(80.0f));
}

void Camera::AddDistance(float delta)
{
    distance_ += delta;

    distance_ =
        std::clamp(
            distance_,
            2.0f,
            200.0f);
}

float Camera::GetYaw() const
{
    return yaw_;
}

float Camera::GetPitch() const
{
    return pitch_;
}

float Camera::GetDistance() const
{
    return distance_;
}

XMFLOAT3 Camera::GetPosition() const
{
    // Spherical Coordinate 방식으로
    // Target 주변의 Orbit Camera 위치를 계산한다.
    const float cosPitch =
        std::cos(pitch_);

    XMFLOAT3 position = {};

    position.x =
        target_.x +
        distance_ *
        cosPitch *
        std::sin(yaw_);

    position.y =
        target_.y +
        distance_ *
        std::sin(pitch_);

    position.z =
        target_.z +
        distance_ *
        cosPitch *
        std::cos(yaw_);

    return position;
}

XMMATRIX Camera::GetViewMatrix() const
{
    const XMFLOAT3 position =
        GetPosition();

    const XMVECTOR eye =
        XMLoadFloat3(&position);

    const XMVECTOR target =
        XMLoadFloat3(&target_);

    // World의 +Y를 Up으로 사용한다.
    const XMVECTOR up =
        XMVectorSet(
            0.0f,
            1.0f,
            0.0f,
            0.0f);

    // DirectX Left-Handed 좌표계용 View Matrix.
    return XMMatrixLookAtLH(
        eye,
        target,
        up);
}

XMMATRIX Camera::GetProjectionMatrix() const
{
    // DirectX Left-Handed Perspective Projection.
    return XMMatrixPerspectiveFovLH(
        fieldOfView_,
        aspectRatio_,
        nearPlane_,
        farPlane_);
}
