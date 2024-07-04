#pragma once

namespace benzin
{

    class MemoryWriter
    {
    public:
        explicit MemoryWriter(std::byte* data = nullptr, Bytes64 maxSize = std::numeric_limits<size_t>::max());
        
        void WriteBytes(std::span<const std::byte> data, Bytes64 offset = 0) const;

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

        template <typename T>
        void WriteArray(std::span<const T> elements, size_t offsetElement = 0) const
        {
            WriteBytes(std::as_bytes(elements), offsetElement * sizeof(T));
        }

    private:
        std::byte* m_Data;
        Bytes64 m_MaxSize;
    };

} // namespace benzin
