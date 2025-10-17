#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/ray_tracing_shader_table.hpp"

#include "benzin/core/buffer_writer.hpp"
#include "benzin/core/math.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/gpu_heap.hpp"

namespace benzin
{

    static void StoreRawShaderIdentifier(const void* rawId, RayTracing_ShaderTable::ShaderIdentifier& outId)
    {
        std::copy_n((const std::byte*)rawId, outId.size(), outId.begin());
    }

    //

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

    void RayTracing_ShaderTable::AllocateBuffer(Device& device)
    {
        // TODO: Replace 'shaderTable' with buffer in default heap

        MakeUniquePtr(m_ShaderTable, device, benzin::BufferCreation
        {
            .DebugName = "RayTracedShadows_ShaderTable",
            .HeapType = GpuHeapType::Upload, // TODO: Replace with default heap
            .ElementSizeInBytes = sizeof(std::byte),
            .ElementCount = GetRequiredTableSizeInBytes(),
        });

        BufferWriter tableWriter = MakeBufferWriter(*m_ShaderTable);

        const auto processIdentifier = [this, &tableWriter](ShaderIdentifier id, GpuAddress& outGpuAddress)
        {
            outGpuAddress.GpuVirtualAddress = m_ShaderTable->GetGpuVirtualAddress() + tableWriter.GetPositionInBytes();
            outGpuAddress.SizeInBytes = id.size();

            tableWriter.WriteData(ToSpan(id.data(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
        };

        processIdentifier(m_RayGenerationShader, m_GpuAddresses.RayGenerationShader);
        processIdentifier(m_MissShader, m_GpuAddresses.MissTable);
        processIdentifier(m_HitGroupShaders, m_GpuAddresses.HitGroupTable);
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

}
