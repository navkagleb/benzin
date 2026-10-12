#pragma once

#include <benzin/graphics/pso.hpp>

namespace benzin
{

    class Buffer;

    class RayTracing_ShaderTable
    {
    public:
        using ShaderIdentifier = std::array<std::byte, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES>;

        struct GpuAddress
        {
            uint64_t m_GpuVirtualAddress = 0;
            uint64_t m_SizeInBytes = 0;
        };

        struct GpuAddresses
        {
            GpuAddress m_RayGenerationShader;
            GpuAddress m_MissTable;
            GpuAddress m_HitGroupTable;
        };

        ~RayTracing_ShaderTable();

        const auto& GetGpuAddresses() const { return m_GpuAddresses; };

        void SetRayGenerationShader(const void* rawId);
        void SetMissShader(const void* rawId);
        void SetHitGroupShaders(const void* rawId);

        void AllocateBuffer(Device& device, std::string_view debugName);

    private:
        uint32_t GetRequiredTableSizeInBytes() const;

        ShaderIdentifier m_RayGenerationShader = {};
        ShaderIdentifier m_MissShader = {}; // TODO: For now supported only one record per table
        ShaderIdentifier m_HitGroupShaders = {}; // TODO: For now supported only one record per table

        std::unique_ptr<Buffer> m_ShaderTable;
        GpuAddresses m_GpuAddresses;
    };

    class RayTracing_Pso : public PsoBase
    {
    public:
        explicit RayTracing_Pso(Device& device);
        ~RayTracing_Pso() override;

        auto* GetD3D12StateObject() const { return m_D3D12StateObject; }
        const auto& GetShaderTable() const { return m_ShaderTable; }

        void Compile(std::string_view debugName) override;
        void Release() override;
        std::span<const ShaderInfo> GetShaders() const override;

        void SetShaderLibrary(ShaderInfo&& library, ShaderBytecode bytecode);
        void SetRayGenerationShader(std::string_view entryPoint);
        void SetMissShader(std::string_view entryPoint);
        void SetHitGroup(std::string_view hitGroupName, std::string_view closestHitEntryPoint);
        void SetShaderConfig(uint32_t payloadSizeInBytes, uint32_t attributeSizeInBytes);

        void ChangeShaderLibrary(ShaderBytecode bytecode);

    private:
        ID3D12StateObject* m_D3D12StateObject = nullptr;

        D3D12_GLOBAL_ROOT_SIGNATURE m_D3D12GlobalRootSignature = {};
        D3D12_DXIL_LIBRARY_DESC m_D3D12DxilLibrary = {};
        D3D12_HIT_GROUP_DESC m_D3D12HitGroup = {};
        D3D12_RAYTRACING_SHADER_CONFIG m_D3D12ShaderConfig = {};
        D3D12_RAYTRACING_PIPELINE_CONFIG1 m_D3D12PipelineConfig = {};

        std::wstring m_RayGenerationEntryPoint;
        std::wstring m_MissShaderEntryPoint;
        std::wstring m_HitGroupName;
        std::wstring m_ClosestHitEntryPoint;

        ShaderInfo m_ShaderLibrary;

        RayTracing_ShaderTable m_ShaderTable;
        std::unique_ptr<Buffer> m_ShaderTableBuffer;
    };

}
