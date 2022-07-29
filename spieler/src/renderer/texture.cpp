#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/texture.hpp"

#include "spieler/renderer/device.hpp"

namespace spieler::renderer
{

    std::vector<SubresourceData> Texture2DResource::LoadFromDDSFile(Device& device, const std::wstring& filename)
    {
        SPIELER_ASSERT(device.GetNativeDevice());
        SPIELER_ASSERT(!filename.empty());

        std::vector<SubresourceData> subresources;

        // Create texture using Desc from DDS file
        SPIELER_ASSERT(SUCCEEDED(DirectX::LoadDDSTextureFromFile(
            device.GetNativeDevice().Get(),
            filename.c_str(),
            &m_Resource,
            m_Data,
            *reinterpret_cast<std::vector<D3D12_SUBRESOURCE_DATA>*>(&subresources)
        )));

        // #TODO: Register texture throw device
        //*this = device.RegisterTexture(std::move(m_Resource));

        return subresources;
    }

    void Texture2DResource::SetResource(ComPtr<ID3D12Resource>&& resource)
    {
        m_Resource = std::move(resource);
    }

    void Texture2D::Reset()
    {
        m_Texture2DResource.Reset();
        m_Views.clear();
    }

} // namespace spieler::renderer
