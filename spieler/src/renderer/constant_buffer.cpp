#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/constant_buffer.hpp"

#include "spieler/core/assert.hpp"

#include "spieler/renderer/renderer.hpp"
#include "spieler/renderer/mapped_data.hpp"

namespace spieler::renderer
{
    
    // ConstantBufferSlice
    ConstantBufferSlice::ConstantBufferSlice(ConstantBuffer* constantBuffer, uint32_t offset)
        : m_ConstantBuffer(constantBuffer)
        , m_Offset(offset)
    {}

    void ConstantBufferSlice::Bind(Context& context, uint32_t rootParameterIndex) const
    {
        SPIELER_ASSERT(m_ConstantBuffer);

        context.GetNativeCommandList()->SetGraphicsRootConstantBufferView(
            rootParameterIndex,
            static_cast<uint64_t>(m_ConstantBuffer->m_Resource->GetGPUVirtualAddress()) + m_Offset
        );
    }

    void ConstantBufferSlice::Update(const void* data, uint32_t size)
    {
        MappedData mappedData{ *m_ConstantBuffer->m_Resource };

        std::memcpy(mappedData.GetPointer<std::byte>() + m_Offset, data, size);
    }

    // ConstantBuffer
    void ConstantBuffer::SetResource(const std::shared_ptr<BufferResource>& resource)
    {
        SPIELER_ASSERT(resource);

        m_Resource = resource;
    }

    bool ConstantBuffer::HasSlice(const void* key) const
    {
        return m_Slices.contains(key);
    }

    ConstantBufferSlice& ConstantBuffer::GetSlice(const void* key)
    {
        SPIELER_ASSERT(m_Resource.get());
        SPIELER_ASSERT(m_Slices.contains(key));

        return m_Slices[key];
    }

    const ConstantBufferSlice& ConstantBuffer::GetSlice(const void* key) const
    {
        SPIELER_ASSERT(m_Resource.get());
        SPIELER_ASSERT(m_Slices.contains(key));

        return m_Slices.at(key);
    }

    void ConstantBuffer::SetSlice(const void* key)
    {
        SPIELER_ASSERT(m_Resource.get());
        SPIELER_ASSERT(!m_Slices.contains(key));

        const ConstantBufferSlice slice{ this, m_Resource->GetStride() * static_cast<uint32_t>(m_Slices.size()) };

        m_Slices[key] = slice;
    }
    
} // namespace spieler::renderer