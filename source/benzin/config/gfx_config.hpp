#pragma once

#include "benzin/graphics/format.hpp"

namespace benzin
{

    struct GfxConfig
    {
        BenzinDefineNonConstructable(GfxConfig);

        static const uint32_t s_MaxRtvDescriptorCount;
        static const uint32_t s_MaxDsvDescriptorCount;
        static const uint32_t s_MaxResourceDescriptorCount;
        static const uint32_t s_MaxSamplerDescriptorCount;

        static const uint32_t s_ConstantBufferAlignment;
        static const uint32_t s_StructuredBufferAlignment;
        static const uint32_t s_TextureAlignment;
        static const uint32_t s_RayTracingShaderRecordAlignment;

        static const Bytes32 s_ShaderIdentifierSize;

        static const bool s_IsShaderDebugEnabled;
        static const bool s_IsShaderSymbolsEnabled;

        static const std::filesystem::path s_ShaderSourceDir;
        static const std::filesystem::path s_ShaderPdbDir;
        static const std::filesystem::path s_ShaderDxilDir;
    };

}
