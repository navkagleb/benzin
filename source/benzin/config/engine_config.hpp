#pragma once

#if defined(BENZIN_DEBUG_BUILD)
    #define BENZIN_IS_DEBUG_BUILD 1
    #define BENZIN_IS_RELEASE_BUILD 0
#elif defined(BENZIN_RELEASE_BUILD)
    #define BENZIN_IS_DEBUG_BUILD 0
    #define BENZIN_IS_RELEASE_BUILD 1
#else
    #error Unknown build type
#endif

#if defined(BENZIN_PLATFORM_WINDOWS)
    #define BENZIN_IS_PLATFORM_WINDOWS 1
#else
    #error Unknown platform
#endif

namespace benzin::config
{

    const std::filesystem::path g_TextureDirPath{ "assets/textures/" };
    const std::filesystem::path g_AbsTextureDirPath = std::filesystem::absolute(g_TextureDirPath);

    const std::filesystem::path g_ModelDirPath{ "assets/models/" };
    const std::filesystem::path g_AbsModelDirPath = std::filesystem::absolute(g_TextureDirPath);

} // namespace benzin::config
