#pragma once

#include "benzin/graphics/dxc_shader_compiler.hpp"

namespace benzin
{

    class ShaderManager
    {
    public:
        std::span<const std::byte> GetShaderDxil(ShaderType shaderType, std::string_view fileName, std::string_view entryPoint) const;
        std::span<const std::byte> GetLibraryDxil(std::string_view fileName) const;

    private:
        const DxcShaderCompiler m_DxcShaderCompiler;

        using ShaderDxilMap = std::unordered_map<uint64_t, std::vector<std::byte>>;
        mutable ShaderDxilMap m_ShaderDxils;
    };

}
