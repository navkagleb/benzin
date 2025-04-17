#pragma once

namespace benzin
{

    bool SaveTextureArrayToDds(std::span<const std::string_view> fileNames, std::string_view outputFileName);

}
