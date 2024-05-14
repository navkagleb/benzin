#pragma once

#include "benzin/graphics/format.hpp"

namespace benzin::config
{

    constexpr uint32_t g_MaxRtvDescriptorCount = 1'000'000;
    constexpr uint32_t g_MaxDsvDescriptorCount = 1'000'000;
    constexpr uint32_t g_MaxResourceDescriptorCount = 1'000'000;
    constexpr uint32_t g_MaxSamplerDescriptorCount = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

    constexpr uint32_t g_ConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    constexpr uint32_t g_StructuredBufferAlignment = sizeof(DirectX::XMFLOAT4);
    constexpr uint32_t g_TextureAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    constexpr uint32_t g_RayTracingShaderRecordAlignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

    constexpr uint32_t g_ShaderIdentifierSizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    constexpr bool g_IsShaderDebugEnabled = BENZIN_IS_DEBUG_BUILD;
    constexpr bool g_IsShaderSymbolsEnabled = BENZIN_IS_DEBUG_BUILD;

} // namespace benzin::config
