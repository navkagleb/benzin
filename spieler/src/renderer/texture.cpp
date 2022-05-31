#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/texture.hpp"

#include "spieler/renderer/device.hpp"
#include "spieler/renderer/context.hpp"

namespace spieler::renderer
{

    bool Texture2DResource::InitAsDepthStencil(Device& device, const Texture2DConfig& config, const DepthStencilClearValue& depthStencilClearValue)
    {
        SPIELER_RETURN_IF_FAILED(device.CreateTexture(config, depthStencilClearValue, *this));

        return true;
    }

    bool Texture2DResource::LoadFromDDSFile(Device& device, Context& context, const std::wstring& filename, UploadBuffer& uploadBuffer)
    {
        SPIELER_ASSERT(!filename.empty());

        std::vector<D3D12_SUBRESOURCE_DATA> subresources;

        SPIELER_RETURN_IF_FAILED(DirectX::LoadDDSTextureFromFile(
            device.GetNativeDevice().Get(),
            filename.c_str(),
            &m_Resource,
            m_Image.Data,
            subresources
        ));

        context.Copy(subresources, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, uploadBuffer, *this);

        SetDebugName(filename);

        return true;
    }

    void Texture2DResource::SetResource(ComPtr<ID3D12Resource>&& resource)
    {
        m_Resource = std::move(resource);
    }

    void Texture2D::Reset()
    {
        m_Resource.Reset();
        m_Views.clear();
    }

} // namespace spieler::renderer