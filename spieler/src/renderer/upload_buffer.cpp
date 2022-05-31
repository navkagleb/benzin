#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/upload_buffer.hpp"

namespace spieler::renderer
{

    namespace _internal
    {

        inline static uint32_t Align(uint32_t value, uint32_t alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }

    } // namespace _internal

    UploadBuffer::UploadBuffer(UploadBuffer&& other) noexcept
        : Resource(std::move(other))
        , m_MappedData(std::exchange(other.m_MappedData, nullptr))
        , m_Size(std::exchange(other.m_Size, 0))
        , m_Stride(std::exchange(other.m_Stride, 0))
    {}

    UploadBuffer::~UploadBuffer()
    {
        if (m_Resource)
        {
            m_Resource->Unmap(0, nullptr);
        }

        m_MappedData = nullptr;
    }

    uint32_t UploadBuffer::Allocate(uint32_t size, uint32_t alignment)
    {
        SPIELER_ASSERT(m_Resource);

        uint32_t offset = alignment == 0 ? m_Marker : _internal::Align(m_Marker, alignment);

        SPIELER_ASSERT(offset + size <= m_Size);

        m_Marker = offset + size;

        return offset;
    }

    UploadBuffer& UploadBuffer::operator =(UploadBuffer&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_Resource = std::exchange(other.m_Resource, nullptr);
        m_MappedData = std::exchange(other.m_MappedData, nullptr);
        m_Size = std::exchange(other.m_Size, 0);
        m_Stride = std::exchange(other.m_Stride, 0);

        return *this;
    }

} // namespace spieler::renderer