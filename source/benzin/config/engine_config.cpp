#include "benzin/config/bootstrap.hpp"
#include "benzin/config/engine_config.hpp"

namespace benzin
{

    const std::filesystem::path EngineConfig::s_TextureDir = std::filesystem::absolute("assets/textures/").make_preferred();
    const std::filesystem::path EngineConfig::s_ModelDir = std::filesystem::absolute("assets/models/").make_preferred();

}
