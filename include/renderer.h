#pragma once
#include <cstdint>
#include "stdafx.h"


namespace bdr
{
    struct RenderConfig
    {
        uint16_t width;
        uint16_t height;
    };

    class Renderer
    {
    public:
        ~Renderer();
        void init(HWND windowHandle);

        RenderConfig m_renderConfig;
        HWND m_windowHandle = nullptr;

    };
}