#include <iostream>
#include <cstdlib>

#include "stdafx.h"
#include "app.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    bdr::RenderConfig config{
        1280,
        720,
    };

    bdr::App app{ config };

    try {
        app.run(hInstance);
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
