#pragma once
#include "dx_helpers.h"
#include "Camera.h"

namespace bdr
{
    class FPSCameraController
    {
    public:
        FPSCameraController() = default;
        FPSCameraController(Camera* camera, float speed, float sensitivity);

        void update(const float deltaTime);

    private:
        Camera* m_camera = nullptr;


        float m_speed = 1.0f;
        float m_sensitivity = 1.0f;

        float m_pitch = 0.0f;
        float m_yaw = DirectX::XM_PIDIV2;

        DirectX::XMVECTOR m_velocity = { 0.0f, 0.0f, 0.0f, 0.0f };
    };
}

