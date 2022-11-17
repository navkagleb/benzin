#pragma once

#include "spieler/graphics/resource.hpp"

namespace spieler
{

    class MappedData
    {
    public:
        MappedData(Resource* resource, uint32_t subresourceIndex);
        MappedData(const std::shared_ptr<Resource>& resource, uint32_t subresourceIndex);

    public:
        ~MappedData();

    public:
        void Write(const void* data, uint64_t size, uint64_t offset = 0);

        std::byte* GetData() { return m_Data; }

    private:
        Resource* m_Resource{ nullptr };
        uint32_t m_SubresourceIndex{ 0 };
        std::byte* m_Data{ nullptr };
    };

} // namespace spieler
