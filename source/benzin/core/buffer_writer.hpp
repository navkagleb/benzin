#pragma once

namespace benzin
{

    using ByteBuffer = std::span<std::byte>;
    using ConstByteBuffer = std::span<const std::byte>;

    class BufferWriter
    {
    public:
        explicit BufferWriter(ByteBuffer targetBuffer, uint64_t position = 0);
        BufferWriter(std::byte* targetBuffer, uint64_t bufferSize, uint64_t position = 0);
        BufferWriter(const BufferWriter&) = delete;

        uint64_t GetPosition() { return m_BufferPosition; }
        void SetPosition(uint64_t position) { m_BufferPosition = position; }

        void WriteData(ConstByteBuffer data);

        void SetElementPosition(uint64_t offsetElement, uint64_t elementSizeInBytes)
        {
            SetPosition(offsetElement * elementSizeInBytes);
        }

        template <typename T>
        void SetElementPosition(uint64_t offsetElement)
        {
            SetPosition(offsetElement * sizeof(T));
        }

        template <typename T>
        void WriteRaw(const T& type)
        {
            const std::span data{ (std::byte*)&type, sizeof(T) };
            WriteData(data);
        }

    private:
        ByteBuffer m_TargetBuffer;
        uint64_t m_BufferPosition = 0;
    };

}
