#pragma once

#include "benzin/graphics/api/buffer.hpp"

namespace benzin
{

    class MappedData
    {
    public:
        explicit MappedData(const BufferResource& bufferResource);
        ~MappedData();

    public:
        template <typename T>
        void Write(const T& data, size_t startElement = 0)
        {
            Write(std::span{ &data, 1 }, startElement * sizeof(T));
        }

        template <typename T>
        void Write(std::span<const T> data, size_t offsetInBytes = 0)
        {
            const size_t dataSizeInBytes = data.size_bytes();

#if BENZIN_DEBUG_BUILD
            const size_t bufferSizeInBytes = m_BufferResource.GetConfig().ElementSize * m_BufferResource.GetConfig().ElementCount;
            BENZIN_ASSERT(offsetInBytes + dataSizeInBytes <= bufferSizeInBytes);
#endif

            memcpy(m_Data + offsetInBytes, data.data(), dataSizeInBytes);
        }

    private:
        const BufferResource& m_BufferResource;
        std::byte* m_Data{ nullptr };
    };

} // namespace benzin
