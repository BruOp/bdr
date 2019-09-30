#pragma once
#include "stdafx.h"
#include "renderer.h"


namespace bdr
{
    class App
    {
    public:
        App(const RenderConfig& renderConfig) noexcept :
            m_renderConfig(renderConfig),
            m_renderer{ renderConfig },
            m_hwnd(nullptr)
        { }

        void run(HINSTANCE hInstance);

        void update();
        void render();

        void onKeyDown(uint8_t key);

        RenderConfig m_renderConfig;
        Renderer m_renderer;

        static LRESULT CALLBACK windowProcedure(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam);

    private:
        void initWindow(HINSTANCE hInstance);

        HWND m_hwnd = nullptr;
    };
}