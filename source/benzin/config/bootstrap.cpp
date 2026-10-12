#include <benzin/config/bootstrap.hpp>

namespace benzin
{

    const std::filesystem::path& GetShaderSourceDir()
    {
        static const std::filesystem::path s_Dir = std::filesystem::absolute("source/shaders").make_preferred();
        return s_Dir;
    }

    const std::filesystem::path& GetShaderPdbDir()
    {
#if BENZIN_IS_DEBUG_BUILD
        static const std::filesystem::path s_Dir = std::filesystem::absolute("bin/shader_dxil_debug").make_preferred();
#else
        static const std::filesystem::path s_Dir = std::filesystem::absolute("bin/shader_dxil_release").make_preferred();
#endif
        return s_Dir;
    }

    const std::filesystem::path& GetShaderDxilDir()
    {
        static const std::filesystem::path s_Dir = std::filesystem::absolute("bin/shader_pbd").make_preferred();
        return s_Dir;
    }

    const std::filesystem::path& GetTextureDir()
    {
        static const std::filesystem::path s_Dir = std::filesystem::absolute("assets/textures/").make_preferred();
        return s_Dir;
    }

    const std::filesystem::path& GetModelDir()
    {
        static const std::filesystem::path s_Dir = std::filesystem::absolute("assets/models/").make_preferred();
        return s_Dir;
    }

}
