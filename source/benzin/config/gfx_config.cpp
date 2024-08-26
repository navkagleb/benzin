#include "benzin/config/bootstrap.hpp"
#include "benzin/config/gfx_config.hpp"

namespace benzin
{

    const uint32_t GfxConfig::s_MaxRtvDescriptorCount = 1'000'000;
    const uint32_t GfxConfig::s_MaxDsvDescriptorCount = 1'000'000;
    const uint32_t GfxConfig::s_MaxResourceDescriptorCount = 1'000'000;
    const uint32_t GfxConfig::s_MaxSamplerDescriptorCount = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

    const Bytes32 GfxConfig::s_ConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    const Bytes32 GfxConfig::s_StructuredBufferAlignment = sizeof(DirectX::XMFLOAT4);
    const Bytes32 GfxConfig::s_TextureAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    const Bytes32 GfxConfig::s_RayTracingShaderRecordAlignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

    const Bytes32 GfxConfig::s_ShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    const bool GfxConfig::s_IsShaderDebugEnabled = BENZIN_IS_DEBUG_BUILD;
    const bool GfxConfig::s_IsShaderSymbolsEnabled = BENZIN_IS_DEBUG_BUILD;

    const std::filesystem::path GfxConfig::s_ShaderSourceDir = std::filesystem::absolute("source/shaders").make_preferred();
    const std::filesystem::path GfxConfig::s_ShaderDxilDir = std::filesystem::absolute(BENZIN_IS_DEBUG_BUILD ? "bin/shader_dxil_debug" : "bin/shader_dxil_release").make_preferred();
    const std::filesystem::path GfxConfig::s_ShaderPdbDir = std::filesystem::absolute("bin/shader_pbd").make_preferred();

}
