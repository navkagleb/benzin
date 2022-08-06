#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/resource.hpp"

namespace spieler::renderer
{

    Resource::Resource(ComPtr<ID3D12Resource>&& dx12Resource)
        : m_DX12Resource{ std::exchange(dx12Resource, nullptr) }
    {}

    Resource::Resource(Resource&& other) noexcept
        : m_DX12Resource{ std::exchange(other.m_DX12Resource, nullptr) }
    {}

    void Resource::Release()
    {
        m_DX12Resource.Reset();
    }

    Resource& Resource::operator=(Resource&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_DX12Resource.Swap(std::move(other.m_DX12Resource));

        return *this;
    }

} // namespace spieler::renderer
