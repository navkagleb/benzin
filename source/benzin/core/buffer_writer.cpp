#include "benzin/config/bootstrap.hpp"
#include "benzin/core/buffer_writer.hpp"

namespace benzin
{

    BufferWriter::BufferWriter(ByteBuffer targetBuffer, uint64_t position)
        : m_TargetBuffer{ targetBuffer }
        , m_BufferPosition{ position }
    {}

    BufferWriter::BufferWriter(std::byte* targetBuffer, uint64_t bufferSize, uint64_t position)
        : BufferWriter{ std::span{ targetBuffer, bufferSize }, position }
    {}

    void BufferWriter::WriteData(ConstByteBuffer data)
    {
        BenzinAssert(m_BufferPosition + data.size() <= m_TargetBuffer.size());

        memcpy(m_TargetBuffer.data() + m_BufferPosition, data.data(), data.size());
        m_BufferPosition += data.size();
    }

}
