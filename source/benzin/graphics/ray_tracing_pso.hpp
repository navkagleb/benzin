#pragma once

#include "benzin/graphics/pso.hpp"
#include "benzin/graphics/ray_tracing_shader_table.hpp"

namespace benzin
{

    class RayTracing_Pso : public PsoBase
    {
    public:
        explicit RayTracing_Pso(Device& device);
        ~RayTracing_Pso() override;

        void Compile(std::string_view debugName) override;
        void Release() override;
        std::span<const ShaderInfo> GetShaders() const override;

        auto* GetD3D12StateObject() const { return m_D3D12StateObject; }
        const auto& GetShaderTable() const { return m_ShaderTable; }

        void SetShaderLibrary(ShaderInfo&& library, ShaderBytecode bytecode);
        void SetRayGenerationShader(std::string_view entryPoint);
        void SetMissShader(std::string_view entryPoint);
        void SetHitGroup(std::string_view hitGroupName, std::string_view closestHitEntryPoint);
        void SetShaderConfig(uint32_t payloadSizeInBytes, uint32_t attributeSizeInBytes);

        void ChangeShaderLibrary(ShaderBytecode bytecode);

    private:
        void BuildShaderTable();

    private:
        ID3D12StateObject* m_D3D12StateObject = nullptr;

        D3D12_GLOBAL_ROOT_SIGNATURE m_D3D12GlobalRootSignature{};
        D3D12_DXIL_LIBRARY_DESC m_D3D12DxilLibrary{};
        D3D12_HIT_GROUP_DESC m_D3D12HitGroup{};
        D3D12_RAYTRACING_SHADER_CONFIG m_D3D12ShaderConfig{};
        D3D12_RAYTRACING_PIPELINE_CONFIG1 m_D3D12PipelineConfig{};

        std::wstring m_RayGenerationEntryPoint;
        std::wstring m_MissShaderEntryPoint;
        std::wstring m_HitGroupName;
        std::wstring m_ClosestHitEntryPoint;

        ShaderInfo m_ShaderLibrary;

        RayTracing_ShaderTable m_ShaderTable;
        std::unique_ptr<Buffer> ShaderTableBuffer;
    };

}
