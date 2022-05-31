#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/resource.hpp"

#include "spieler/core/assert.hpp"

#include "spieler/renderer/device.hpp"

namespace spieler::renderer
{

    Resource::Resource(Resource&& other) noexcept
        : m_Resource(std::exchange(other.m_Resource, nullptr))
    {}

    void Resource::SetDebugName(const std::wstring& name)
    {
        SPIELER_ASSERT(m_Resource);

        m_Resource->SetName(name.c_str());
    }

    void Resource::Reset()
    {
        m_Resource.Reset();
    }

    Resource& Resource::operator =(Resource&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_Resource = std::exchange(other.m_Resource, nullptr);

        return *this;
    }

} // namespace spieler::renderer