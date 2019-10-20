#pragma once
#include "stdafx.h"
#include <chrono>

#include "GameInput.h"
#include "renderer.h"
#include "FPSCameraController.h"


namespace bdr
{
    class App
    {
    public:
        App(const RenderConfig& renderConfig) noexcept :
            m_renderConfig(renderConfig),
            m_renderer{ renderConfig },
            m_hwnd(nullptr),
            m_startTime{},
            m_lastFrame{ 0.0f }
        { }
        ~App()
        {
            GameInput::Shutdown();
        }

        void run();

        void update();
        void render();

        RenderConfig m_renderConfig;
        Renderer m_renderer;
        FPSCameraController m_controller;

        static LRESULT CALLBACK windowProcedure(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam);

    private:
        void initWindow(HINSTANCE hInstance);

        HWND m_hwnd = nullptr;

        std::chrono::time_point<std::chrono::high_resolution_clock> m_startTime;
        float m_lastFrame = 0.0f;
    };
}