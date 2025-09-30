#pragma once

namespace benzin
{

    namespace GraphicsConfig
    {
        inline constexpr uint32_t g_FrameInFlightCount = 3;
        inline constexpr uint32_t g_ReadbackLatency = g_FrameInFlightCount + 1;

        inline constexpr uint32_t g_MaxRtvDescriptorCount = 1'000'000;
        inline constexpr uint32_t g_MaxDsvDescriptorCount = 1'000'000;
        inline constexpr uint32_t g_MaxResourceDescriptorCount = 1'000'000;
        inline constexpr uint32_t g_MaxSamplerDescriptorCount = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

        inline constexpr uint32_t g_ConstBufferAlignmentInBytes = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        inline constexpr uint32_t g_StructuredBufferAlignmentInBytes = sizeof(DirectX::XMFLOAT4);
        inline constexpr uint32_t g_TextureAlignmentInBytes = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
        inline constexpr uint32_t g_RayTracingShaderRecordAlignmentInBytes = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
        inline constexpr uint32_t g_ShaderIdentifierSizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

        inline constexpr bool g_IsShaderDebugEnabled = BENZIN_IS_DEBUG_BUILD;
        inline constexpr bool g_IsShaderSymbolsEnabled = BENZIN_IS_DEBUG_BUILD;

        const std::filesystem::path& GetShaderSourceDir();
        const std::filesystem::path& GetShaderPdbDir();
        const std::filesystem::path& GetShaderDxilDir();
    };

}
