#pragma once

#include "benzin/graphics/format.hpp"

namespace benzin
{

    namespace GraphicsConfig
    {
        uint32_t GetMaxRtvDescriptorCount();
        uint32_t GetMaxDsvDescriptorCount();
        uint32_t GetMaxResourceDescriptorCount();
        uint32_t GetMaxSamplerDescriptorCount();

        uint32_t GetConstBufferAlignmentInBytes();
        uint32_t GetStructuredBufferAlignmentInBytes();
        uint32_t GetTextureAlignmentInBytes();
        uint32_t GetRayTracingShaderRecordAlignmentInBytes();
        uint32_t GetShaderIdentifierSizeInBytes();

        bool IsShaderDebugEnabled();
        bool IsShaderSymbolsEnabled();

        const std::filesystem::path& GetShaderSourceDir();
        const std::filesystem::path& GetShaderPdbDir();
        const std::filesystem::path& GetShaderDxilDir();
    };

}
