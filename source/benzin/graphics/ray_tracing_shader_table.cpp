#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/ray_tracing_shader_table.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/math.hpp"
#include "benzin/graphics/buffer.hpp"

namespace benzin
{

    constexpr uint64_t g_RecordAlignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
    constexpr uint64_t g_TableAlignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

    static void StoreRawShaderIdentifier(const void* rawId, RayTracingShaderTable::ShaderIdentifier& outId)
    {
        std::copy_n((const std::byte*)rawId, outId.size(), outId.begin());
    }

    //

    void RayTracingShaderTable::SetRayGenerationShader(const void* rawId)
    {
        StoreRawShaderIdentifier(rawId, m_RayGenerationShader);
    }

    void RayTracingShaderTable::SetMissShader(const void* rawId)
    {
        StoreRawShaderIdentifier(rawId, m_MissShader);
    }

    void RayTracingShaderTable::SetHitGroupShaders(const void* rawId)
    {
        StoreRawShaderIdentifier(rawId, m_HitGroupShaders);
    }

    Bytes64 RayTracingShaderTable::GetRequiredTableSize() const
    {
        const auto getIdentifierSize = [](ShaderIdentifier id)
        {
             const uint64_t recordSize = AlignUp(id.size(), g_RecordAlignment);
             return AlignUp(recordSize, g_TableAlignment);
        };

        uint64_t tableSize = 0;
        tableSize += getIdentifierSize(m_RayGenerationShader);
        tableSize += getIdentifierSize(m_MissShader);
        tableSize += getIdentifierSize(m_HitGroupShaders);

        return tableSize;
    }

    void RayTracingShaderTable::UploadToGpu(Buffer* shaderTable)
    {
        // TODO: Replace 'shaderTable' with buffer in default heap

        BenzinAssert(shaderTable->GetMemoryType() == ResourceMemoryType::Upload);
        BenzinAssert(m_ShaderTable == nullptr);
        m_ShaderTable = shaderTable;

        const benzin::MemoryWriter tableWriter{ m_ShaderTable->GetCpuMappedData(), m_ShaderTable->GetSize() };
        uint64_t offset = 0;

        const auto processIdentifier = [this, &tableWriter, &offset](ShaderIdentifier id, GpuAddress& outGpuAddress)
        {
            outGpuAddress.GpuVirtualAddress = m_ShaderTable->GetGpuVirtualAddress() + offset;
            outGpuAddress.Size = id.size();

            tableWriter.WriteBytes(id, offset);
            offset += g_TableAlignment;
        };

        processIdentifier(m_RayGenerationShader, m_GpuAddresses.RayGenerationShader);
        processIdentifier(m_MissShader, m_GpuAddresses.MissTable);
        processIdentifier(m_HitGroupShaders, m_GpuAddresses.HitGroupTable);
    }

}
