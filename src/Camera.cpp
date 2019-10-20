#include "Camera.h"

using namespace DirectX;

namespace bdr
{
    Camera::Camera() :
        direction{ 0.0f, 0.0f, -1.0f, 0.0f },
        position{ 0.0f, 0.0f, 0.0f, 1.0f },
        view{},
        projection{},
        FoV{ XMConvertToRadians(60.0f) },
        aspectRatio{ 16.0f / 9.0f },
        near_{ 0.1f },
        far_{ 1000.0f }
    {
        updateView();
        updateProjection(FoV, aspectRatio, near_, far_);
    }

    Camera::Camera(const float FoV, const float aspectRatio, const float near_, const float far_) :
        direction{ 0.0f, 0.0f, -1.0f, 0.0f },
        position{ 0.0f, 0.0f, 0.0f, 1.0f },
        view{},
        projection{},
        FoV{ FoV },
        aspectRatio{ aspectRatio },
        near_{ near_ },
        far_{ far_ }
    {
        updateView();
        updateProjection(FoV, aspectRatio, near_, far_);
    }

    void Camera::updateDirection(XMVECTOR newDirection)
    {
        direction = XMVector3Normalize(newDirection);
        XMVECTOR worldUp = XMVectorGetY(direction) == 1.0f ? XMVECTOR{ 0.0f, 0.0f, 1.0f, 0.0f } : XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f };
        right = XMVector3Normalize(XMVector3Cross(direction, worldUp));
        up = XMVector3Normalize(XMVector3Cross(direction, right));
    }

    void Camera::updateView()
    {
        XMVECTOR worldUp = XMVectorGetY(direction) != 1.0f ? XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f } : XMVECTOR{ 1.0f, 0.0f, 0.0f, 0.0f };

        view = XMMatrixLookAtRH(position, XMVectorAdd(position, direction), worldUp);
    }

    void Camera::updateProjection(const float newAspectRatio)
    {
        aspectRatio = newAspectRatio;
        projection = XMMatrixPerspectiveFovRH(FoV, newAspectRatio, near_, far_);
    }


    void Camera::updateProjection(const float newFov, const float newAspectRatio, const float newNear, const float newFar)
    {
        FoV = XMConvertToRadians(newFov);
        aspectRatio = newAspectRatio;
        near_ = newNear;
        far_ = newFar;
        projection = XMMatrixPerspectiveFovRH(FoV, newAspectRatio, near_, far_);
    }

    XMFLOAT4X4 Camera::getViewAsFloat4x4()
    {
        XMFLOAT4X4 dst{};
        XMStoreFloat4x4(&dst, view);
        return dst;
    }

    DirectX::XMFLOAT4X4 Camera::getProjectionAsFloat4x4()
    {
        XMFLOAT4X4 dst{};
        XMStoreFloat4x4(&dst, projection);
        return dst;
    }

    DirectX::XMFLOAT4X4 Camera::getViewProjectionAsFloat4x4()
    {
        XMFLOAT4X4 dst{};
        XMStoreFloat4x4(&dst, getViewProjection());
        return dst;
    }

}