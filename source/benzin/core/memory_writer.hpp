#pragma once

namespace benzin
{

    class MemoryWriter
    {
    public:
        explicit MemoryWriter(std::byte* data = nullptr, size_t maxSizeInBytes = std::numeric_limits<size_t>::max());
        
        void WriteBytes(std::span<const std::byte> data, size_t offsetInBytes = 0) const;

        template <typename T>
        void WriteSized(const T& data, size_t elementSize, size_t offsetElement = 0) const
        {
            WriteBytes(std::as_bytes(std::span{ &data, 1 }), offsetElement * elementSize);
        }

        template <typename T>
        void Write(const T& data, size_t offsetElement = 0) const
        {
            WriteSized(data, sizeof(T), offsetElement);
        }

    private:
        std::byte* m_Data;
        size_t m_MaxSizeInBytes;
    };

} // namespace benzin
