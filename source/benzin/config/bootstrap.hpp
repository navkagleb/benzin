#pragma once

#include <benzin/config/win64_includes.hpp> // Include win64 first of all because other dependencies uses #include <Windows.h>

#if defined(BENZIN_DEBUG_BUILD)
    #define BENZIN_DEBUG_BUILD_ENABLED 1
#elif defined(BENZIN_RELEASE_BUILD)
    #define BENZIN_DEBUG_BUILD_ENABLED 0
#else
    #error Unknown build type
#endif

#include <benzin/config/d3d12_includes.hpp>
#include <benzin/config/std_includes.hpp>
#include <benzin/config/third_party_includes.hpp>

#include <benzin/utility/benzin_defines.hpp>
#include <benzin/utility/file_utils.hpp>
#include <benzin/utility/string_utils.hpp>

#include <benzin/core/debug.hpp>

#include <benzin/core/bytes.hpp>
#include <benzin/core/common.hpp>
#include <benzin/core/enum_flags.hpp>
#include <benzin/core/timers.hpp>

#if !defined(BENZIN_FRAME_COUNT)
    #define BENZIN_FRAME_COUNT 3
    #define BENZIN_READBACK_LATENCY (BENZIN_FRAME_COUNT + 1)
#endif

#if BENZIN_DEBUG_BUILD_ENABLED
    #define BENZIN_SHADER_SYMBOLS_ENABLED 1
#else
    #define BENZIN_SHADER_SYMBOLS_ENABLED 0
#endif

namespace benzin
{

    const std::filesystem::path& GetShaderSourceDir();
    const std::filesystem::path& GetShaderPdbDir();
    const std::filesystem::path& GetShaderDxilDir();

    const std::filesystem::path& GetTextureDir();
    const std::filesystem::path& GetModelDir();

}
