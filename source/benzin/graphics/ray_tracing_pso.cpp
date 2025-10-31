#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/ray_tracing_pso.hpp>

#include <benzin/core/math.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

namespace benzin
{

    static void StoreRawShaderIdentifier(const void* rawId, RayTracing_ShaderTable::ShaderIdentifier& outId)
    {
        std::copy_n((const std::byte*)rawId, outId.size(), outId.begin());
    }

    // RayTracing_ShaderTable

    RayTracing_ShaderTable::~RayTracing_ShaderTable() = default;

    void RayTracing_ShaderTable::SetRayGenerationShader(const void* rawId)
    {
        StoreRawShaderIdentifier(rawId, m_RayGenerationShader);
    }

    void RayTracing_ShaderTable::SetMissShader(const void* rawId)
    {
        StoreRawShaderIdentifier(rawId, m_MissShader);
    }

    void RayTracing_ShaderTable::SetHitGroupShaders(const void* rawId)
    {
        StoreRawShaderIdentifier(rawId, m_HitGroupShaders);
    }

    void RayTracing_ShaderTable::AllocateBuffer(Device& device, std::string_view debugName)
    {
        // TODO: Replace 'shaderTable' with buffer in default heap

        MakeUniquePtr(m_ShaderTable, device, BufferCreation
        {
            .m_DebugName = std::format("RayTracingShaderTable::{}", debugName),
            .m_HeapType = GpuHeapType::Upload, // TODO: Replace with default heap
            .m_ElementSizeInBytes = sizeof(std::byte),
            .m_ElementCount = GetRequiredTableSizeInBytes(),
        });

        BufferWriter tableWriter = MakeBufferWriter(*m_ShaderTable);

        const auto processIdentifier = [this, &tableWriter](ShaderIdentifier id, GpuAddress& outGpuAddress)
        {
            outGpuAddress.m_GpuVirtualAddress = m_ShaderTable->GetGpuVirtualAddress() + tableWriter.GetPositionInBytes();
            outGpuAddress.m_SizeInBytes = id.size();

            tableWriter.WriteData(ToSpan(id.data(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
        };

        processIdentifier(m_RayGenerationShader, m_GpuAddresses.m_RayGenerationShader);
        processIdentifier(m_MissShader, m_GpuAddresses.m_MissTable);
        processIdentifier(m_HitGroupShaders, m_GpuAddresses.m_HitGroupTable);
    }

    uint32_t RayTracing_ShaderTable::GetRequiredTableSizeInBytes() const
    {
        const auto getIdentifierSizeInBytes = [](ShaderIdentifier id)
        {
            const auto recordSizeInBytes = AlignUp((uint32_t)id.size(), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
            return AlignUp(recordSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        };

        uint32_t tableSizeInBytes = 0;
        tableSizeInBytes += getIdentifierSizeInBytes(m_RayGenerationShader);
        tableSizeInBytes += getIdentifierSizeInBytes(m_MissShader);
        tableSizeInBytes += getIdentifierSizeInBytes(m_HitGroupShaders);

        return (uint32_t)tableSizeInBytes;
    }

    // RayTracing_Pso

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
        Release();
    }

    void RayTracing_Pso::Compile(std::string_view debugName)
    {
        BenzinAssert(m_D3D12StateObject == nullptr);

        const auto d3d12StateSubObjects = std::to_array(
        {
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &m_D3D12GlobalRootSignature },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &m_D3D12DxilLibrary },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &m_D3D12HitGroup },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &m_D3D12ShaderConfig },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &m_D3D12PipelineConfig },
        });

        D3D12_STATE_OBJECT_DESC d3d12StateObjectDesc = {};
        d3d12StateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        d3d12StateObjectDesc.NumSubobjects = (uint32_t)d3d12StateSubObjects.size();
        d3d12StateObjectDesc.pSubobjects = d3d12StateSubObjects.data();

        BenzinD3D12Call(m_Device.GetD3D12Device()->CreateStateObject(&d3d12StateObjectDesc, IID_PPV_ARGS(&m_D3D12StateObject)));
        SetD3DObjectDebugName(m_D3D12StateObject, debugName);

        ComPtr<ID3D12StateObjectProperties> d3d12StateObjectProperties;
        BenzinD3D12Call(m_D3D12StateObject->QueryInterface(IID_PPV_ARGS(&d3d12StateObjectProperties)));

        m_ShaderTable.SetRayGenerationShader(d3d12StateObjectProperties->GetShaderIdentifier(m_RayGenerationEntryPoint.c_str()));
        m_ShaderTable.SetMissShader(d3d12StateObjectProperties->GetShaderIdentifier(m_MissShaderEntryPoint.c_str()));
        m_ShaderTable.SetHitGroupShaders(d3d12StateObjectProperties->GetShaderIdentifier(m_HitGroupName.c_str()));
        m_ShaderTable.AllocateBuffer(m_Device, debugName);
    }

    void RayTracing_Pso::Release()
    {
        m_Device.DeferredRelease(m_D3D12StateObject);
        m_D3D12StateObject = nullptr;
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

}
