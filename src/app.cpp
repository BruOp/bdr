#include "app.h"

namespace bdr
{
    void App::run()
    {
        OutputDebugString(L"Starting App!\n");
        
        Microsoft::WRL::Wrappers::RoInitializeWrapper InitializeWinRT(RO_INIT_MULTITHREADED);
        ASSERT_SUCCEEDED(InitializeWinRT);

        HINSTANCE hInstance = GetModuleHandle(0);

        initWindow(hInstance);
        m_renderer.init(m_hwnd);
        ShowWindow(m_hwnd, SW_SHOW);

        // Main sample loop.
        MSG msg = {};
        while (msg.message != WM_QUIT) {
            // Process any messages in the queue.
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    void App::update()
    {
        m_renderer.onUpdate();
    }

    void App::render()
    {
        m_renderer.onRender();
    }

    void App::onKeyDown(uint8_t key)
    {
        std::wstring keyCode = L"Key pressed! Honk! " + std::to_wstring(key) + L"\n";
        OutputDebugString(keyCode.c_str());
    }

    LRESULT App::windowProcedure(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam)
    {
        //DXSample* pSample = reinterpret_cast<DXSample*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        App* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        switch (message) {
            case WM_CREATE:
            {
                LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
                SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
            }
            return 0;

        case WM_KEYDOWN:
            if (app) {
                app->onKeyDown(static_cast<uint8_t>(wParam));
            }
            return 0;

            //case WM_KEYUP:
            //    if (app) {
            //        app->onKeyUp(static_cast<UINT8>(wParam));
            //    }
            //    return 0;

        case WM_PAINT:
            if (app) {
                app->update();
                app->render();
            }
            return 0;

        case WM_SIZE:
            if (app) {
                RECT clientRect = {};
                GetClientRect(app->m_hwnd, &clientRect);
                UINT16_MAX;
                uint16_t width = static_cast<uint16_t>(clientRect.right) - static_cast<uint16_t>(clientRect.left);
                uint16_t height = static_cast<uint16_t>(clientRect.bottom) - static_cast<uint16_t>(clientRect.top);

                app->m_renderer.onResize(width, height);
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        // Handle any messages the switch statement didn't.
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    void App::initWindow(HINSTANCE hInstance)
    {
        WNDCLASSEX windowClass = { 0 };
        windowClass.cbSize = sizeof(WNDCLASSEX);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = windowProcedure;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.lpszClassName = L"BDRAppClass";
        RegisterClassEx(&windowClass);

        RECT windowRect = { 0, 0, static_cast<LONG>(m_renderConfig.width), static_cast<LONG>(m_renderConfig.height) };
        AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

        m_hwnd = CreateWindow(
            windowClass.lpszClassName,
            L"BDR",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            nullptr,
            nullptr,
            hInstance,
            this
        );
    }
}