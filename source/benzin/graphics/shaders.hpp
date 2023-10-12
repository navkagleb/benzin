#pragma once

namespace benzin
{

    struct ShaderCreation;

    enum class ShaderType : uint8_t
    {
        Vertex,
        Pixel,
        Compute,
    };

    std::span<const std::byte> GetShaderBinary(ShaderType shaderType, const ShaderCreation& shaderCreation);
    std::span<const std::byte> GetLibraryBinary(std::string_view fileName);

} // namespace benzin
