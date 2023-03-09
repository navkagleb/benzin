#pragma once

#include "benzin/graphics/api/format.hpp"

namespace benzin::config
{

    constexpr uint32_t GetBackBufferCount() { return 3; }
    constexpr GraphicsFormat GetBackBufferFormat() { return GraphicsFormat::R8G8B8A8UnsignedNorm; }

    constexpr const char* GetVertexShaderTarget() { return "vs_6_6"; }
    constexpr const char* GetHullShaderTarget() { return "hs_6_6"; }
    constexpr const char* GetDomainShaderTarget() { return "ds_6_6"; }
    constexpr const char* GetGeometryShaderTarget() { return "gs_6_6"; }
    constexpr const char* GetPixelShaderTarget() { return "ps_6_6"; }
    constexpr const char* GetComputeShaderTarget() { return "cs_6_6"; }

    constexpr uint32_t GetMaxRenderTargetViewDescriptorCount() { return 1'000'000; }
    constexpr uint32_t GetMaxDepthStencilViewDescriptorCount() { return 1'000'000; }
    constexpr uint32_t GetMaxResourceDescriptorCount() { return 1'000'000; }
    constexpr uint32_t GetMaxSamplerDescriptorCount() { return D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE; }

    constexpr const char* GetShaderPath() { return "../shaders/"; }

} // namespace benzin::config
