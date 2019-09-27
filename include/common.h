#pragma once
#include <iostream>

namespace bdr
{
    inline void debugLog(const char* msg)
    {
    #ifndef NDEBUG
        std::cout << msg << std::endl;
    #endif
    }

    inline void debugLog(const std::string& msg)
    {
    #ifndef NDEBUG
        std::cout << msg << std::endl;
    #endif
    }
}
