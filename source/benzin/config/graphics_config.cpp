#include <benzin/config/bootstrap.hpp>
#include <benzin/config/graphics_config.hpp>

namespace benzin
{

    const std::filesystem::path& GraphicsConfig::GetShaderSourceDir()
    {
        static const std::filesystem::path g_ShaderSourceDir = std::filesystem::absolute("source/shaders").make_preferred();
        return g_ShaderSourceDir;
    }

    const std::filesystem::path& GraphicsConfig::GetShaderPdbDir()
    {
#if BENZIN_IS_DEBUG_BUILD
        static const std::filesystem::path g_ShaderDxilDir = std::filesystem::absolute("bin/shader_dxil_debug").make_preferred();
#else
        static const std::filesystem::path g_ShaderDxilDir = std::filesystem::absolute("bin/shader_dxil_release").make_preferred();
#endif
        return g_ShaderDxilDir;
    }

    const std::filesystem::path& GraphicsConfig::GetShaderDxilDir()
    {
        static const std::filesystem::path g_ShaderPdbDir = std::filesystem::absolute("bin/shader_pbd").make_preferred();
        return g_ShaderPdbDir;
    }

}
