#pragma once

#include "benzin/graphics/format.hpp"

namespace benzin::config
{

    constexpr uint32_t g_MainAdapterIndex = 0;

    constexpr uint32_t g_BackBufferCount = 3;
    constexpr GraphicsFormat g_BackBufferFormat = GraphicsFormat::RGBA8Unorm;

    constexpr std::wstring_view g_VertexShaderTarget = L"vs_6_6";
    constexpr std::wstring_view g_PixelShaderTarget = L"ps_6_6";
    constexpr std::wstring_view g_ComputeShaderTarget = L"cs_6_6";
    constexpr std::wstring_view g_ShaderLibraryTarget = L"lib_6_6";

    constexpr uint32_t g_MaxRenderTargetViewDescriptorCount = 1'000'000;
    constexpr uint32_t g_MaxDepthStencilViewDescriptorCount = 1'000'000;
    constexpr uint32_t g_MaxResourceDescriptorCount = 1'000'000;
    constexpr uint32_t g_MaxSamplerDescriptorCount = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

    constexpr uint32_t g_ConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    constexpr uint32_t g_TextureAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;

    constexpr bool g_IsShaderDebugEnabled = BENZIN_IS_DEBUG_BUILD;
    constexpr bool g_IsShaderSymbolsEnabled = BENZIN_IS_DEBUG_BUILD;

    const std::filesystem::path g_ShaderSourceDirectoryPath{ "source/shaders/" };
    const std::filesystem::path g_AbsoluteShaderSourceDirectoryPath = std::filesystem::absolute(g_ShaderSourceDirectoryPath);

#if BENZIN_IS_DEBUG_BUILD
    const std::filesystem::path g_ShaderBinaryDirectoryPath{ "bin/shader_bytecode_debug/" };
#elif BENZIN_IS_RELEASE_BUILD
    const std::filesystem::path g_ShaderBinaryDirectoryPath{ "bin/shader_bytecode_release/" };
#endif

    const std::filesystem::path g_AbsoluteShaderBinaryDirectoryPath = std::filesystem::absolute(g_ShaderBinaryDirectoryPath);

    const std::filesystem::path g_ShaderDebugDirectoryPath{ "bin/shader_pbd/" };
    const std::filesystem::path g_AbsoluteShaderDebugDirectoryPath = std::filesystem::absolute(g_ShaderDebugDirectoryPath);

} // namespace benzin::config
