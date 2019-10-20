#pragma once
#include <stdafx.h>

namespace bdr
{
    struct Camera
    {
        Camera();
        Camera(const float FoV, const float aspectRatio, const float near_, const float far_);

        void updateDirection(DirectX::XMVECTOR newDirection);
        void updateView();
        void updateProjection(const float aspectRatio);
        void updateProjection(const float FoV, const float aspectRatio, const float near_, const float far_);

        DirectX::XMFLOAT4X4 getViewAsFloat4x4();
        DirectX::XMFLOAT4X4 getProjectionAsFloat4x4();
        DirectX::XMFLOAT4X4 getViewProjectionAsFloat4x4();

        inline void storeViewAsFloat4x4(DirectX::XMFLOAT4X4* dst)
        {
            XMStoreFloat4x4(dst, view);
        };
        inline void storeProjectionAsFloat4x4(DirectX::XMFLOAT4X4* dst)
        {
            XMStoreFloat4x4(dst, projection);
        };
        inline void storeViewProjectionAsFloat4x4(DirectX::XMFLOAT4X4* dst)
        {
            XMStoreFloat4x4(dst, getViewProjection());
        };

        inline DirectX::XMMATRIX getViewProjection()
        {
            return XMMatrixMultiply(view, projection);
        };

        // Directions in world space
        DirectX::XMVECTOR direction;
        DirectX::XMVECTOR right;
        DirectX::XMVECTOR up;
        DirectX::XMVECTOR position;

        DirectX::XMMATRIX view;
        DirectX::XMMATRIX projection;

        float FoV;
        float aspectRatio;
        float near_;
        float far_;
    };
}

