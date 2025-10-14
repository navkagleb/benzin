#include <benzin/config/bootstrap.hpp>
#include <benzin/config/engine_config.hpp>

namespace benzin
{

    const std::filesystem::path& EngineConfig::GetTextureDir()
    {
        static std::filesystem::path s_Dir = std::filesystem::absolute("assets/textures/").make_preferred();
        return s_Dir;
    }

    const std::filesystem::path& EngineConfig::GetModelDir()
    {
        static std::filesystem::path s_Dir = std::filesystem::absolute("assets/models/").make_preferred();
        return s_Dir;
    }

}
