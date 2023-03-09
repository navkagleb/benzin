#pragma once

#include "benzin/graphics/api/buffer.hpp"

namespace benzin
{

    class MappedData
    {
    public:
        explicit MappedData(BufferResource& bufferResource);
        ~MappedData();

    public:
        void Write(const void* data, uint64_t size, uint64_t offset = 0);

        std::byte* GetData() { return m_Data; }

    private:
        BufferResource& m_BufferResource;
        std::byte* m_Data{ nullptr };
    };

} // namespace benzin
