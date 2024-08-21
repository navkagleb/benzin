#pragma once

namespace benzin
{

    struct EngineConfig
    {
        BenzinDefineNonConstructable(EngineConfig);

        static const std::filesystem::path s_TextureDir;
        static const std::filesystem::path s_ModelDir;
    };

}
