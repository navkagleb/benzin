#include <benzin/config/bootstrap.hpp>
#include <benzin/core/buffer_writer.hpp>

#include <benzin/core/math.hpp>
#include <benzin/graphics/buffer.hpp>

namespace benzin
{

    // BufferWriter

    BufferWriter::BufferWriter(std::byte* targetBuffer, uint64_t bufferSize)
        : m_TargetBuffer{ targetBuffer, bufferSize }
    {}

    void BufferWriter::ResetTargetBuffer(std::byte* data, uint64_t sizeInBytes)
    {
        m_TargetBuffer = std::span{ data, sizeInBytes };
        m_BufferPositionInBytes = 0;
    }

    void BufferWriter::SetPositionInBytes(uint64_t positionInBytes)
    {
        BenzinAssert(positionInBytes < m_TargetBuffer.size());
        m_BufferPositionInBytes = positionInBytes;
    }

    void BufferWriter::SetElementPosition(uint32_t offsetElement, uint64_t elementSizeInBytes)
    {
        SetPositionInBytes(offsetElement * elementSizeInBytes);
    }

    void BufferWriter::WriteData(std::span<const std::byte> data)
    {
        BenzinAssert(m_BufferPositionInBytes + data.size() <= m_TargetBuffer.size());

        memcpy(m_TargetBuffer.data() + m_BufferPositionInBytes, data.data(), data.size());
        m_BufferPositionInBytes += data.size();
    }

    // BufferReader

    BufferReader::BufferReader(const std::byte* data, uint64_t sizeInBytes)
        : m_TargetBuffer{ data, sizeInBytes }
    {}

    void BufferReader::SetBufferSizeInBytes(uint64_t sizeInBytes)
    {
        BenzinAssert(sizeInBytes <= m_TargetBuffer.size_bytes());

        m_TargetBuffer = std::span{ m_TargetBuffer.data(), sizeInBytes };
    }

    void BufferReader::AlignBufferPositionInBytes(uint64_t alignmentInBytes)
    {
        m_BufferPositionInBytes = AlignUp(m_BufferPositionInBytes, alignmentInBytes);
    }

    bool BufferReader::IsValidToRead(uint64_t sizeInBytes) const
    {
        return m_BufferPositionInBytes + sizeInBytes <= m_TargetBuffer.size_bytes();
    }

    BufferReader BufferReader::MakeSubReader(uint64_t subSizeInBytes)
    {
        BufferReader subReader{ m_TargetBuffer.data() + m_BufferPositionInBytes, subSizeInBytes };

        m_BufferPositionInBytes += subSizeInBytes;

        return subReader;
    }

    BufferWriter MakeBufferWriter(const Buffer& buffer)
    {
        BenzinAssert(buffer.GetCpuMappedData() != nullptr);

        return BufferWriter{ buffer.GetCpuMappedData(), buffer.GetSizeInBytes() };
    }

}
