#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/resource.hpp"

namespace spieler::renderer
{

    Resource::Resource(ID3D12Resource* dx12Resource)
        : m_DX12Resource{ dx12Resource }
    {}

    Resource::~Resource()
    {
        if (m_DX12Resource)
        {
            m_DX12Resource->Release();
        }
    }

    GPUVirtualAddress Resource::GetGPUVirtualAddress() const
    {
        SPIELER_ASSERT(m_DX12Resource);

        return m_DX12Resource->GetGPUVirtualAddress();
    }

} // namespace spieler::renderer
