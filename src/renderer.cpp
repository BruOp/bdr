#include "renderer.h"

namespace bdr
{
    Renderer::~Renderer()
    {
        OutputDebugString("Cleaning up renderer\n");
    }


    void Renderer::init(HWND windowHandle)
    {
        OutputDebugString("Initializing Renderer!\n");
        m_windowHandle = windowHandle;
    }
}