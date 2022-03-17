#include "renderer/upload_buffer.h"

namespace Spieler
{

    UploadBuffer::UploadBuffer(UploadBuffer&& other) noexcept
        : m_Buffer(std::exchange(other.m_Buffer, nullptr))
        , m_MappedData(std::exchange(other.m_MappedData, nullptr))
        , m_ElementCount(std::exchange(other.m_ElementCount, 0))
        , m_Stride(std::exchange(other.m_Stride, 0))
    {}

    UploadBuffer::~UploadBuffer()
    {
        if (m_Buffer)
        {
            m_Buffer->Unmap(0, nullptr);
        }

        m_MappedData = nullptr;
    }

    UploadBuffer& UploadBuffer::operator =(UploadBuffer&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_Buffer        = std::exchange(other.m_Buffer, nullptr);
        m_MappedData    = std::exchange(other.m_MappedData, nullptr);
        m_ElementCount  = std::exchange(other.m_ElementCount, 0);
        m_Stride        = std::exchange(other.m_Stride, 0);

        return *this;
    }

} // namespace Spieler