#include "FPSCameraController.h"
#include "GameInput.h"

using namespace DirectX;

namespace bdr
{
    FPSCameraController::FPSCameraController(Camera* camera, float speed, float sensitivity) :
        m_camera{ camera },
        m_speed{ speed },
        m_sensitivity{ sensitivity },
        m_pitch{ 0.0f },
        m_yaw{ -XM_PIDIV2 },
        m_velocity{ 0.0f, 0.0f, 0.0f, 0.0f }
    {}

    void FPSCameraController::update(const float deltaTime)
    {
        float yaw = GameInput::GetAnalogInput(GameInput::kAnalogMouseX) * m_sensitivity;
        float pitch = GameInput::GetAnalogInput(GameInput::kAnalogMouseY) * m_sensitivity;

        float strafe =
            (GameInput::IsPressed(GameInput::kKey_d) ? 1.0f : 0.0f) -
            (GameInput::IsPressed(GameInput::kKey_a) ? 1.0f : 0.0f);
        float forward =
            (GameInput::IsPressed(GameInput::kKey_w) ? 1.0f : 0.0f) -
            (GameInput::IsPressed(GameInput::kKey_s) ? 1.0f : 0.0f);
        float ascent =
            (GameInput::IsPressed(GameInput::kKey_q) ? 1.0f : 0.0f) -
            (GameInput::IsPressed(GameInput::kKey_e) ? 1.0f : 0.0f);

        // Update our direction
        m_pitch += pitch;
        m_pitch = XMMin(XM_PIDIV2, m_pitch);
        m_pitch = XMMax(-XM_PIDIV2, m_pitch);

        m_yaw += yaw;

        XMVECTOR front = {
            XMScalarCos(m_yaw) * XMScalarCos(m_pitch),
            XMScalarSin(m_pitch),
            XMScalarCos(m_pitch) * XMScalarSin(m_yaw),
            0.0f
        };
        m_camera->updateDirection(front);

        // Update our position
        m_camera->position += m_speed * deltaTime * (
            forward * m_camera->direction +
            strafe * m_camera->right +
            ascent * m_camera->up
            );

        m_camera->updateView();
    }
}