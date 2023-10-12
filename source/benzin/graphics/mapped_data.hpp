#pragma once

#include "benzin/graphics/buffer.hpp"

namespace benzin
{

    class MappedData
    {
    public:
        explicit MappedData(const Buffer& buffer);
        ~MappedData();

    public:
        template <typename T>
        void Write(const T& data, size_t startElement = 0)
        {
            Write(std::span{ &data, 1 }, startElement * sizeof(T));
        }

        template <typename T>
        void WriteBytes(const T& data, size_t offsetInBytes = 0)
        {
            Write(std::span{ reinterpret_cast<const std::byte*>(&data), sizeof(T) }, offsetInBytes);
        }

        template <typename T>
        void Write(std::span<const T> data, size_t offsetInBytes = 0)
        {
            const size_t dataSizeInBytes = data.size_bytes();

            BenzinAssert(offsetInBytes + dataSizeInBytes <= m_Buffer.GetSizeInBytes());
            memcpy(m_Data + offsetInBytes, data.data(), dataSizeInBytes);
        }

    private:
        const Buffer& m_Buffer;
        std::byte* m_Data = nullptr;
    };

} // namespace benzin
