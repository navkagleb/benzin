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

    const std::vector<std::byte>& GetShaderBlob(
        const std::string& fileName,
        const std::string& entryPoint,
        ShaderType shaderType,
        const std::vector<std::string>& defines
    );

} // namespace benzin
