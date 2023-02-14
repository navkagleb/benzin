#pragma once

#include "benzin/graphics/api/resource.hpp"

namespace benzin
{

    class MappedData
    {
    public:
        explicit MappedData(Resource& resource, uint32_t subresourceIndex = 0);
        ~MappedData();

    public:
        void Write(const void* data, uint64_t size, uint64_t offset = 0);

        std::byte* GetData() { return m_Data; }

    private:
        Resource& m_Resource;
        uint32_t m_SubresourceIndex{ 0 };
        std::byte* m_Data{ nullptr };
    };

} // namespace benzin
