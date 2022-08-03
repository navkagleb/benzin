#pragma once

#include "spieler/renderer/resource.hpp"

namespace spieler::renderer
{

    class MappedData
    {
    public:
        MappedData() = default;
        MappedData(Resource& resource, uint32_t subresourceIndex);
        ~MappedData();

    public:
        std::byte* GetData() { return m_Data; }

    private:
        Resource& m_Resource;
        uint32_t m_SubresourceIndex{ 0 };
        std::byte* m_Data{ nullptr };
    };

} // namespace spieler::renderer
