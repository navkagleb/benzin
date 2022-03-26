#include "renderer/texture.hpp"

#include <DDSTextureLoader12.h>

#include "renderer/descriptor_heap.hpp"

namespace Spieler
{

    bool Texture2D::LoadFromDDSFile(const std::wstring& filename)
    {
        SPIELER_RETURN_IF_FAILED(DirectX::LoadDDSTextureFromFile(
            GetDevice().Get(),
            filename.c_str(),
            &m_Resource,
            m_Data,
            m_SubresourceData
        ));

        return true;
    }

    ShaderResourceView::ShaderResourceView(const Texture2D& texture2D, const DescriptorHeap& descriptorHeap, std::uint32_t index)
    {
        Init(texture2D, descriptorHeap, index);
    }

    void ShaderResourceView::Init(const Texture2D& texture2D, const DescriptorHeap& descriptorHeap, std::uint32_t index)
    {
        m_DescriptorHeap = &descriptorHeap;

        const D3D12_RESOURCE_DESC texture2DDesc{ texture2D.m_Resource->GetDesc() };

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texture2DDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = texture2DDesc.MipLevels;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        srvDesc.Texture2D.MostDetailedMip = 0;
        GetDevice()->CreateShaderResourceView(texture2D.m_Resource.Get(), &srvDesc, m_DescriptorHeap->GetCPUHandle(index));
    }

    void ShaderResourceView::Bind(std::uint32_t rootParameterIndex) const
    {
        SPIELER_ASSERT(m_DescriptorHeap);

        GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, m_DescriptorHeap->GetGPUHandle(m_Index));
    }

} // namespace Spieler