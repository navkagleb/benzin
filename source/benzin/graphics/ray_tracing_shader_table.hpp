#pragma once

namespace benzin
{

    class Buffer;
    class Device;

    class RayTracing_ShaderTable
    {
    public:
        using ShaderIdentifier = std::array<std::byte, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES>;

        struct GpuAddress
        {
            uint64_t GpuVirtualAddress = 0;
            uint64_t SizeInBytes = 0;
        };

        struct GpuAddresses
        {
            GpuAddress RayGenerationShader;
            GpuAddress MissTable;
            GpuAddress HitGroupTable;
        };

        ~RayTracing_ShaderTable();

        const auto& GetGpuAddresses() const { return m_GpuAddresses; };

        void SetRayGenerationShader(const void* rawId);
        void SetMissShader(const void* rawId);
        void SetHitGroupShaders(const void* rawId);

        void AllocateBuffer(Device& device);

    private:
        uint32_t GetRequiredTableSizeInBytes() const;

    private:
        ShaderIdentifier m_RayGenerationShader{};
        ShaderIdentifier m_MissShader{}; // TODO: For now supported only one record per table
        ShaderIdentifier m_HitGroupShaders{}; // TODO: For now supported only one record per table

        std::unique_ptr<Buffer> m_ShaderTable;
        GpuAddresses m_GpuAddresses;
    };

}
