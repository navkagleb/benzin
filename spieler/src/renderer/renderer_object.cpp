#include "renderer/renderer_object.hpp"

#include "application.hpp"

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

} // namespace Spieler