#include "benzin/config/bootstrap.hpp"
#include "benzin/config/graphics_config.hpp"

namespace benzin
{

    static const uint32_t g_MaxRtvDescriptorCount = 1'000'000;
    static const uint32_t g_MaxDsvDescriptorCount = 1'000'000;
    static const uint32_t g_MaxResourceDescriptorCount = 1'000'000;
    static const uint32_t g_MaxSamplerDescriptorCount = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

    static const uint32_t g_ConstantBufferAlignmentInBytes = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    static const uint32_t g_StructuredBufferAlignmentInBytes = sizeof(DirectX::XMFLOAT4);
    static const uint32_t g_TextureAlignmentInBytes = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    static const uint32_t g_RayTracingShaderRecordAlignmentInBytes = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
    static const uint32_t g_ShaderIdentifierSizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    static const bool g_IsShaderDebugEnabled = BENZIN_IS_DEBUG_BUILD;
    static const bool g_IsShaderSymbolsEnabled = BENZIN_IS_DEBUG_BUILD;

    static const std::filesystem::path g_ShaderSourceDir = std::filesystem::absolute("source/shaders").make_preferred();
    static const std::filesystem::path g_ShaderDxilDir = std::filesystem::absolute(BENZIN_IS_DEBUG_BUILD ? "bin/shader_dxil_debug" : "bin/shader_dxil_release").make_preferred();
    static const std::filesystem::path g_ShaderPdbDir = std::filesystem::absolute("bin/shader_pbd").make_preferred();

#define BenzinImplGraphicsConfigArg_WithDecay(functionName, value) \
    std::decay_t<decltype(value)> GraphicsConfig::functionName() { return value; }

#define BenzinImplGraphicsConfigArg_WithRef(functionName, value) \
    std::add_lvalue_reference_t<decltype(value)> GraphicsConfig::functionName() { return value; }

    BenzinImplGraphicsConfigArg_WithDecay(GetMaxRtvDescriptorCount, g_MaxRtvDescriptorCount)
    BenzinImplGraphicsConfigArg_WithDecay(GetMaxDsvDescriptorCount, g_MaxDsvDescriptorCount)
    BenzinImplGraphicsConfigArg_WithDecay(GetMaxResourceDescriptorCount, g_MaxResourceDescriptorCount)
    BenzinImplGraphicsConfigArg_WithDecay(GetMaxSamplerDescriptorCount, g_MaxSamplerDescriptorCount)

    BenzinImplGraphicsConfigArg_WithDecay(GetConstBufferAlignmentInBytes, g_ConstantBufferAlignmentInBytes)
    BenzinImplGraphicsConfigArg_WithDecay(GetStructuredBufferAlignmentInBytes, g_StructuredBufferAlignmentInBytes)
    BenzinImplGraphicsConfigArg_WithDecay(GetTextureAlignmentInBytes, g_TextureAlignmentInBytes)
    BenzinImplGraphicsConfigArg_WithDecay(GetRayTracingShaderRecordAlignmentInBytes, g_RayTracingShaderRecordAlignmentInBytes)
    BenzinImplGraphicsConfigArg_WithDecay(GetShaderIdentifierSizeInBytes, g_ShaderIdentifierSizeInBytes)

    BenzinImplGraphicsConfigArg_WithDecay(IsShaderDebugEnabled, g_IsShaderDebugEnabled)
    BenzinImplGraphicsConfigArg_WithDecay(IsShaderSymbolsEnabled, g_IsShaderSymbolsEnabled)

    BenzinImplGraphicsConfigArg_WithRef(GetShaderSourceDir, g_ShaderSourceDir)
    BenzinImplGraphicsConfigArg_WithRef(GetShaderPdbDir, g_ShaderDxilDir)
    BenzinImplGraphicsConfigArg_WithRef(GetShaderDxilDir, g_ShaderPdbDir)

#undef BenzinImplGraphicsConfigArg_WithDecay
#undef BenzinImplGraphicsConfigArg_WithRef

}
