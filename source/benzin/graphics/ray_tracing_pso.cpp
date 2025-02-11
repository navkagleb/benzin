#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/ray_tracing_pso.hpp"

#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/hr_assert.hpp"
#include "benzin/graphics/unified_root_signature.hpp"

namespace benzin
{

    RayTracing_Pso::RayTracing_Pso(Device& device)
        : PsoBase(device)
    {
        m_D3D12GlobalRootSignature.pGlobalRootSignature = m_Device.GetUnifiedRootSignature().GetD3D12RootSignature();

        m_D3D12DxilLibrary.NumExports = 0;
        m_D3D12DxilLibrary.pExports = nullptr;

        m_D3D12HitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        m_D3D12HitGroup.AnyHitShaderImport = nullptr;
        m_D3D12HitGroup.IntersectionShaderImport = nullptr;

        m_D3D12PipelineConfig.MaxTraceRecursionDepth = 1;
        m_D3D12PipelineConfig.Flags = D3D12_RAYTRACING_PIPELINE_FLAG_SKIP_PROCEDURAL_PRIMITIVES;
    }

    RayTracing_Pso::~RayTracing_Pso()
    {
        m_Device.DeferredRelease(*this);
        m_D3D12StateObject = nullptr;
    }

    void RayTracing_Pso::Compile()
    {
        const auto d3d12StateSubObjects = std::to_array(
        {
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &m_D3D12GlobalRootSignature },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &m_D3D12DxilLibrary },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &m_D3D12HitGroup },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &m_D3D12ShaderConfig },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &m_D3D12PipelineConfig },
        });

        const D3D12_STATE_OBJECT_DESC d3d12StateObjectDesc
        {
            .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
            .NumSubobjects = (uint32_t)d3d12StateSubObjects.size(),
            .pSubobjects = d3d12StateSubObjects.data(),
        };

        BenzinHrEnsure(m_Device.GetD3D12Device()->CreateStateObject(&d3d12StateObjectDesc, IID_PPV_ARGS(&m_D3D12StateObject)));

        BuildShaderTable();
    }

    std::span<const ShaderInfo> RayTracing_Pso::GetShaders() const
    {
        return std::span<const ShaderInfo>{ &m_ShaderLibrary, 1 };
    }

    void RayTracing_Pso::SetShaderLibrary(ShaderInfo&& library, ShaderBytecode bytecode)
    {
        BenzinAssert(library.IsValid() && library.GetType() == ShaderType::Library);
        m_ShaderLibrary = std::move(library);
        
        ChangeShaderLibrary(bytecode);
    }

    void RayTracing_Pso::SetRayGenerationShader(std::string_view entryPoint)
    {
        m_RayGenerationEntryPoint = ToWideString(entryPoint);
    }

    void RayTracing_Pso::SetMissShader(std::string_view entryPoint)
    {
        m_MissShaderEntryPoint = ToWideString(entryPoint);
    }

    void RayTracing_Pso::SetHitGroup(std::string_view hitGroupName, std::string_view closestHitEntryPoint)
    {
        m_HitGroupName = ToWideString(hitGroupName);
        m_ClosestHitEntryPoint = ToWideString(closestHitEntryPoint);

        m_D3D12HitGroup.HitGroupExport = m_HitGroupName.data();
        m_D3D12HitGroup.ClosestHitShaderImport = m_ClosestHitEntryPoint.data();
    }

    void RayTracing_Pso::SetShaderConfig(uint32_t payloadSizeInBytes, uint32_t attributeSizeInBytes)
    {
        m_D3D12ShaderConfig.MaxPayloadSizeInBytes = std::max<uint32_t>(4u, payloadSizeInBytes), // Min size is 4 bytes
        m_D3D12ShaderConfig.MaxAttributeSizeInBytes = attributeSizeInBytes; // Barycentrics
    }

    void RayTracing_Pso::ChangeShaderLibrary(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        m_D3D12DxilLibrary.DXILLibrary.pShaderBytecode = bytecode.data();
        m_D3D12DxilLibrary.DXILLibrary.BytecodeLength = bytecode.size();
    }

    void RayTracing_Pso::BuildShaderTable()
    {
        BenzinEnsure(m_D3D12StateObject != nullptr);

        ComPtr<ID3D12StateObjectProperties> d3d12StateObjectProperties;
        BenzinHrEnsure(m_D3D12StateObject->QueryInterface(IID_PPV_ARGS(&d3d12StateObjectProperties)));

        m_ShaderTable.SetRayGenerationShader(d3d12StateObjectProperties->GetShaderIdentifier(m_RayGenerationEntryPoint.c_str()));
        m_ShaderTable.SetMissShader(d3d12StateObjectProperties->GetShaderIdentifier(m_MissShaderEntryPoint.c_str()));
        m_ShaderTable.SetHitGroupShaders(d3d12StateObjectProperties->GetShaderIdentifier(m_HitGroupName.c_str()));
        m_ShaderTable.AllocateBuffer(m_Device);
    }

}
