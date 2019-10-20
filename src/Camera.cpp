#include "Camera.h"

using namespace DirectX;

namespace bdr
{
    Camera::Camera() :
        direction{ 0.0f, 0.0f, -1.0f, 0.0f },
        position{ 0.0f, 0.0f, 0.0f, 1.0f },
        view{},
        projection{},
        FoV{ 60.0f },
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

    void Camera::updateView()
    {
        XMVECTOR up = XMVectorGetY(direction) != 1.0f ? XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f } : XMVECTOR{ 1.0f, 0.0f, 0.0f, 0.0f };

        view = XMMatrixLookAtRH(position, XMVectorAdd(position, direction), up);
    }
    
    void Camera::updateProjection(const float newAspectRatio)
    { 
        aspectRatio = newAspectRatio;
        projection = XMMatrixPerspectiveFovRH(FoV, newAspectRatio, near_, far_);
    }


    void Camera::updateProjection(const float FoV, const float newAspectRatio, const float near_, const float far_)
    {
        this->FoV = FoV;
        aspectRatio = newAspectRatio;
        this->near_ = near_;
        this->far_ = far_;
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