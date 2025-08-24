#include "benzin/config/bootstrap.hpp"
#include "benzin/core/buffer_writer.hpp"

namespace benzin
{

    BufferWriter::BufferWriter(ByteBuffer targetBuffer, uint64_t positionInBytes)
        : m_TargetBuffer{ targetBuffer }
        , m_BufferPositionInBytes{ positionInBytes }
    {}

    BufferWriter::BufferWriter(std::byte* targetBuffer, uint64_t bufferSize, uint64_t positionInBytes)
        : BufferWriter{ std::span{ targetBuffer, bufferSize }, positionInBytes }
    {}

    void BufferWriter::ResetTargetBuffer(ByteBuffer targetBuffer)
    {
        m_TargetBuffer = targetBuffer;
        m_BufferPositionInBytes = 0;
    }

    void BufferWriter::SetPositionInBytes(uint64_t positionInBytes)
    {
        BenzinAssert(positionInBytes < m_TargetBuffer.size());
        m_BufferPositionInBytes = positionInBytes;
    }


    void BufferWriter::WriteData(ConstByteBuffer data)
    {
        BenzinAssert(m_BufferPositionInBytes + data.size() <= m_TargetBuffer.size());

        memcpy(m_TargetBuffer.data() + m_BufferPositionInBytes, data.data(), data.size());
        m_BufferPositionInBytes += data.size();
    }

}
