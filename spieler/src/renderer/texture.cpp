#include "renderer/texture.hpp"

#include <d3dx12.h>

#include <DDSTextureLoader12.h>

#include "renderer/resource.hpp"
#include "renderer/upload_buffer.hpp"
#include "renderer/descriptor_heap.hpp"

namespace Spieler
{

    bool Texture2D::LoadFromDDSFile(const std::wstring& filename, UploadBuffer& uploadBuffer)
    {
        SPIELER_ASSERT(!filename.empty());

        SPIELER_RETURN_IF_FAILED(DirectX::LoadDDSTextureFromFile(
            GetDevice().Get(),
            filename.c_str(),
            &m_Texture,
            m_Data,
            m_SubresourceData
        ));

        const std::uint64_t uploadBufferSize{ GetRequiredIntermediateSize(m_Texture.Get(), 0, m_SubresourceData.size()) };

        SPIELER_RETURN_IF_FAILED(uploadBuffer.Init<std::byte>(UploadBufferType::Default, uploadBufferSize));

        UpdateSubresources(GetCommandList().Get(), m_Texture.Get(), static_cast<ID3D12Resource*>(uploadBuffer), 0, 0, m_SubresourceData.size(), m_SubresourceData.data());

        const CD3DX12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(m_Texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) };
        GetCommandList()->ResourceBarrier(1, &barrier);

        SetDebugName(filename);

        return true;
    }

    ShaderResourceView::ShaderResourceView(const Texture2D& texture2D, const DescriptorHeap& descriptorHeap, std::uint32_t index)
    {
        Init(texture2D, descriptorHeap, index);
    }

    void ShaderResourceView::Init(const Texture2D& texture2D, const DescriptorHeap& descriptorHeap, std::uint32_t index)
    {
        m_DescriptorHeap = &descriptorHeap;

        const D3D12_RESOURCE_DESC texture2DDesc{ texture2D.m_Texture->GetDesc() };

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texture2DDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = texture2DDesc.MipLevels;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        srvDesc.Texture2D.MostDetailedMip = 0;
        GetDevice()->CreateShaderResourceView(texture2D.m_Texture.Get(), &srvDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ m_DescriptorHeap->GetDescriptorCPUHandle(index) });
    }

    void ShaderResourceView::Bind(std::uint32_t rootParameterIndex) const
    {
        SPIELER_ASSERT(m_DescriptorHeap);

        GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE{ m_DescriptorHeap->GetDescriptorGPUHandle(m_Index) });
    }

} // namespace Spieler