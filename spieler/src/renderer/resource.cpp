#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/resource.hpp"

namespace spieler::renderer
{

    Resource::Resource(ComPtr<ID3D12Resource>&& dx12Resource)
        : m_DX12Resource{ std::exchange(dx12Resource, nullptr) }
    {}

} // namespace spieler::renderer
