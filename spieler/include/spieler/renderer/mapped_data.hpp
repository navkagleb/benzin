#pragma once

#include "spieler/renderer/resource.hpp"

namespace spieler::renderer
{

    class MappedData
    {
    public:
        explicit MappedData(Resource& resource);
        ~MappedData();

    public:
        template <typename T = void>
        T* GetPointer() { return reinterpret_cast<T*>(m_Data); }

    private:
        Resource& m_Resource;
        std::byte* m_Data{ nullptr };
    };

} // namespace spieler::renderer