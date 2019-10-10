#include <cstdlib>

#include "stdafx.h"
#include "app.h"

//_Use_decl_annotations_
//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
int wmain(/*int argc, wchar_t** argv*/)
{
    bdr::RenderConfig config{
        1280,
        720,
        L"",
    };

    bdr::App app{ config };

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::string cs(e.what());
        std::wstring ws;
        std::copy(cs.begin(), cs.end(), back_inserter(ws));
        OutputDebugString(ws.c_str());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
