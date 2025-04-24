#pragma once

namespace benzin
{

    using ByteBuffer = std::span<std::byte>;
    using ConstByteBuffer = std::span<const std::byte>;

    class BufferWriter
    {
    public:
        explicit BufferWriter(ByteBuffer targetBuffer, uint64_t positionInBytes = 0);
        BufferWriter(std::byte* targetBuffer, uint64_t bufferSizeInBytes, uint64_t positionInBytes = 0);
        BufferWriter(const BufferWriter&) = delete;

        auto GetPositionInBytes() { return m_BufferPositionInBytes; }
        void SetPositionInBytes(uint64_t positionInBytes) { m_BufferPositionInBytes = positionInBytes; }

        void WriteData(ConstByteBuffer data);

        void SetElementPosition(uint32_t offsetElement, uint64_t elementSizeInBytes)
        {
            SetPositionInBytes(offsetElement * elementSizeInBytes);
        }

        template <typename T>
        void SetElementPosition(uint32_t offsetElement)
        {
            SetPositionInBytes(offsetElement * sizeof(T));
        }

        template <typename T>
        void WriteRaw(const T& type)
        {
            const std::span data{ (std::byte*)&type, sizeof(T) };
            WriteData(data);
        }

        template <typename T>
        void WriteArray(std::span<const T> array)
        {
            WriteData(std::as_bytes(array));
        }

    private:
        ByteBuffer m_TargetBuffer;
        uint64_t m_BufferPositionInBytes = 0;
    };

}
