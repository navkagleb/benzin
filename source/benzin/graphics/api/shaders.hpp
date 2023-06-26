#pragma once

namespace benzin
{

    struct ShaderDefine
    {
        std::string_view Name;
        std::string Value;
    };

    enum class ShaderType : uint8_t
    {
        Vertex,
        Pixel,
        Compute,
    };

    std::span<const std::byte> GetShaderBlob(
        ShaderType shaderType,
        std::string_view fileName,
        std::string_view entryPoint,
        const std::vector<std::string>& defines
    );

} // namespace benzin
