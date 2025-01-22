#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/ray_tracing_shader_table.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/buffer_writer.hpp"
#include "benzin/core/math.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{

    constexpr uint32_t g_RecordAlignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
    constexpr uint32_t g_TableAlignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

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
            .MemoryType = benzin::ResourceMemoryType::Upload, // TODO: Replace with default heap
            .ElementSize = sizeof(std::byte),
            .ElementCount = GetRequiredTableSizeInBytes(),
        });

        BufferWriter tableWriter{ m_ShaderTable->GetCpuMappedData(), m_ShaderTable->GetSize() };

        const auto processIdentifier = [this, &tableWriter](ShaderIdentifier id, GpuAddress& outGpuAddress)
        {
            outGpuAddress.GpuVirtualAddress = m_ShaderTable->GetGpuVirtualAddress() + tableWriter.GetPosition();
            outGpuAddress.Size = id.size();

            tableWriter.WriteData(std::span{ id.data(), g_TableAlignment });
        };

        processIdentifier(m_RayGenerationShader, m_GpuAddresses.RayGenerationShader);
        processIdentifier(m_MissShader, m_GpuAddresses.MissTable);
        processIdentifier(m_HitGroupShaders, m_GpuAddresses.HitGroupTable);
    }

    uint32_t RayTracing_ShaderTable::GetRequiredTableSizeInBytes() const
    {
        const auto getIdentifierSize = [](ShaderIdentifier id)
        {
            const auto recordSize = AlignUp((uint32_t)id.size(), g_RecordAlignment);
            return AlignUp(recordSize, g_TableAlignment);
        };

        uint32_t tableSize = 0;
        tableSize += getIdentifierSize(m_RayGenerationShader);
        tableSize += getIdentifierSize(m_MissShader);
        tableSize += getIdentifierSize(m_HitGroupShaders);

        return (uint32_t)tableSize;
    }

}
