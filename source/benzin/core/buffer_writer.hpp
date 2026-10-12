#pragma once

namespace benzin
{

    class Buffer;

    class BufferWriter
    {
    public:
        BufferWriter() = default;
        BufferWriter(std::byte* targetBuffer, uint64_t bufferSizeInBytes);

        auto GetPositionInBytes() { return m_BufferPositionInBytes; }

        void ResetTargetBuffer(std::byte* data, uint64_t sizeInBytes);
        void SetPositionInBytes(uint64_t positionInBytes);

        void SetElementPosition(uint32_t offsetElement, uint64_t elementSizeInBytes);
        void WriteData(std::span<const std::byte> data);

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
        std::span<std::byte> m_TargetBuffer;
        uint64_t m_BufferPositionInBytes = 0;
    };

    class BufferReader
    {
    public:
        BufferReader(const std::byte* data, uint64_t sizeInBytes);

        uint64_t GetBufferPositionInBytes() const { return m_BufferPositionInBytes; }

        uint64_t GetBufferSizeInBytes() const { return m_TargetBuffer.size_bytes(); }
        void SetBufferSizeInBytes(uint64_t sizeInBytes);

        void AlignBufferPositionInBytes(uint64_t alignment);

        bool IsValidToRead(uint64_t sizeInBytes) const;

        template <typename T>
        bool IsValidToRead() const { return IsValidToRead(sizeof(T)); }

        template <typename T>
        T ReadRaw()
        {
            BenzinAssert(IsValidToRead<T>());

            const T result = *(T*)(m_TargetBuffer.data() + m_BufferPositionInBytes);

            m_BufferPositionInBytes += sizeof(T);

            return result;
        }

        BufferReader MakeSubReader(uint64_t subSizeInBytes);

    private:
        std::span<const std::byte> m_TargetBuffer;
        uint64_t m_BufferPositionInBytes = 0;
    };

    BufferWriter MakeBufferWriter(const Buffer& buffer);

}
