#pragma once

namespace benzin
{

    struct ShaderCreation;

    enum class ShaderType : uint8_t
    {
        Vertex,
        Pixel,
        Compute,
        Library,
    };

    std::span<const std::byte> GetShaderBinary(ShaderType shaderType, const ShaderCreation& shaderCreation);

} // namespace benzin
