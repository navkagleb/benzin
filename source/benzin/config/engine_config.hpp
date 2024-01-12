#pragma once

namespace benzin::config
{

    const std::filesystem::path g_TextureDirPath{ "assets/textures/" };
    const std::filesystem::path g_AbsTextureDirPath = std::filesystem::absolute(g_TextureDirPath);

    const std::filesystem::path g_ModelDirPath{ "assets/models/" };
    const std::filesystem::path g_AbsModelDirPath = std::filesystem::absolute(g_TextureDirPath);

} // namespace benzin::config
