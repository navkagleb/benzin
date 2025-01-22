#include "benzin/config/bootstrap.hpp"
#include "benzin/core/buffer_writer.hpp"

#include "benzin/core/asserter.hpp"

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
        const bool isValid = m_BufferPosition + data.size() <= m_TargetBuffer.size();
        BenzinAssert(isValid);

        memcpy(m_TargetBuffer.data() + m_BufferPosition, data.data(), data.size());
        m_BufferPosition += data.size();
    }

}
