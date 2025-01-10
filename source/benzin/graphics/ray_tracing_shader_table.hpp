#pragma once

namespace benzin
{

    class Buffer;

    class RayTracingShaderTable
    {
    public:
        using ShaderIdentifier = std::array<std::byte, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES>;

        struct GpuAddress
        {
            uint64_t GpuVirtualAddress = 0;
            Bytes64 Size = 0;
        };

        struct GpuAddresses
        {
            GpuAddress RayGenerationShader;
            GpuAddress MissTable;
            GpuAddress HitGroupTable;
        };

    public:
        const auto& GetGpuAddresses() const { return m_GpuAddresses; };

        void SetRayGenerationShader(const void* rawId);
        void SetMissShader(const void* rawId);
        void SetHitGroupShaders(const void* rawId);

        Bytes64 GetRequiredTableSize() const;
        void UploadToGpu(Buffer* shaderTable);

    private:
        ShaderIdentifier m_RayGenerationShader{};
        ShaderIdentifier m_MissShader{}; // TODO: For now supported only one record per table
        ShaderIdentifier m_HitGroupShaders{}; // TODO: For now supported only one record per table

        Buffer* m_ShaderTable = nullptr;
        GpuAddresses m_GpuAddresses;
    };

}
