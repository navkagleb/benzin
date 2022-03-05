#include "renderer_object.h"

#include "application.h"

namespace Spieler
{

    ComPtr<ID3D12Device>& RendererObject::GetDevice()
    {
        return Application::GetInstance().GetRenderer().m_Device;
    }

    ComPtr<ID3D12GraphicsCommandList>& RendererObject::GetCommandList()
    {
        return Application::GetInstance().GetRenderer().m_CommandList;
    }

    DescriptorHeap& RendererObject::GetMixtureDescriptorHeap()
    {
        return Application::GetInstance().GetRenderer().m_MixtureDescriptorHeap;
    }

} // namespace Spieler