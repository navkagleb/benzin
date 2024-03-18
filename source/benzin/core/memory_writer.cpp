#include "benzin/config/bootstrap.hpp"
#include "benzin/core/memory_writer.hpp"

#include "benzin/core/asserter.hpp"

namespace benzin
{

    MemoryWriter::MemoryWriter(std::byte* data, size_t maxSizeInBytes)
        : m_Data{ data }
        , m_MaxSizeInBytes{ maxSizeInBytes }
    {}

    void MemoryWriter::WriteBytes(std::span<const std::byte> data, size_t offsetInBytes) const
    {
        BenzinAssert(m_Data != nullptr);
        BenzinAssert(offsetInBytes + data.size_bytes() <= m_MaxSizeInBytes);

        memcpy(m_Data + offsetInBytes, data.data(), data.size_bytes());
    }

} // namespace benzin
