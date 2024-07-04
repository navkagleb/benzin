#include "benzin/config/bootstrap.hpp"
#include "benzin/core/memory_writer.hpp"

#include "benzin/core/asserter.hpp"

namespace benzin
{

    MemoryWriter::MemoryWriter(std::byte* data, Bytes64 maxSize)
        : m_Data{ data }
        , m_MaxSize{ maxSize }
    {}

    void MemoryWriter::WriteBytes(std::span<const std::byte> data, Bytes64 offset) const
    {
        BenzinAssert(m_Data != nullptr);
        BenzinAssert(offset + data.size_bytes() <= m_MaxSize);

        memcpy(m_Data + offset, data.data(), data.size_bytes());
    }

} // namespace benzin
