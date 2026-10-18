// [칠판 프로젝트에서의 역할]
// 카메라 위치·시선·투영 행렬을 관리합니다. 입력 자체는 Application에서 해석합니다.

// ============================================================================
// Camera.h
// ----------------------------------------------------------------------------
// View / Projection Matrix와 Orbit Camera 상태를 관리한다.
//
// Camera는 키보드나 마우스를 직접 읽지 않는다.
// 입력 해석은 CameraController가 담당하고,
// Camera는 순수하게 "현재 카메라 상태"만 저장한다.
// ============================================================================

#pragma once

#include <DirectXMath.h>

class Camera
{
public:
    // Perspective Projection 설정.
    void SetPerspective(
        float fieldOfViewRadians,
        float aspectRatio,
        float nearPlane,
        float farPlane);

    // Window Resize 시 Projection의 Aspect Ratio만 변경한다.
    void SetAspectRatio(float aspectRatio);

    // Orbit Camera가 바라보는 중심점.
    void SetTarget(
        const DirectX::XMFLOAT3& target);

    const DirectX::XMFLOAT3& GetTarget() const;

    // Target을 World 좌표 기준으로 이동한다.
    void MoveTarget(
        float deltaX,
        float deltaY,
        float deltaZ);

    // Orbit의 Yaw / Pitch / Distance를 한 번에 설정한다.
    void SetOrbit(
        float yawRadians,
        float pitchRadians,
        float distance);

    // 현재 Orbit 각도에 변화량을 더한다.
    void AddOrbit(
        float yawDelta,
        float pitchDelta);

    // Target과 Camera 사이의 거리를 변경한다.
    void AddDistance(float delta);

    float GetYaw() const;
    float GetPitch() const;
    float GetDistance() const;

    // Target + Orbit 값으로 실제 World Camera Position을 계산한다.
    DirectX::XMFLOAT3 GetPosition() const;

    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix() const;

private:
    // Orbit의 기준점.
    DirectX::XMFLOAT3 target_ =
    {
        0.0f,
        0.0f,
        0.0f
    };

    // Y축을 중심으로 도는 수평 회전값.
    float yaw_ =
        DirectX::XM_PI;

    // 위/아래 회전값.
    float pitch_ =
        DirectX::XMConvertToRadians(25.0f);

    // Target과 Camera 사이 거리.
    float distance_ = 8.0f;

    float fieldOfView_ =
        DirectX::XMConvertToRadians(60.0f);

    float aspectRatio_ =
        16.0f / 9.0f;

    float nearPlane_ = 0.1f;
    float farPlane_ = 1000.0f;
};
