#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/resource_view.hpp"

#include "spieler/renderer/device.hpp"
#include "spieler/renderer/context.hpp"
#include "spieler/renderer/texture.hpp"
#include "spieler/renderer/sampler.hpp"
#include "spieler/renderer/constant_buffer.hpp"

namespace spieler::renderer
{

    namespace _internal
    {

        // Non static because used in root_signature.cpp
        D3D12_FILTER ConvertToD3D12TextureFilter(const TextureFilter& filter)
        {
            const TextureFilterType minification{ filter.Minification };
            const TextureFilterType magnification{ filter.Magnification };
            const TextureFilterType mipLevel{ filter.MipLevel };

            D3D12_FILTER d3d12Filter{ D3D12_FILTER_MIN_MAG_MIP_POINT };

            if (minification == TextureFilterType::Point)
            {
                SPIELER_ASSERT(magnification != TextureFilterType::Anisotropic && mipLevel != TextureFilterType::Anisotropic);

                if (magnification == TextureFilterType::Point)
                {
                    d3d12Filter = mipLevel == TextureFilterType::Point ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                }
                else
                {
                    d3d12Filter = mipLevel == TextureFilterType::Point ? D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
                }
            }
            else if (minification == TextureFilterType::Linear)
            {
                SPIELER_ASSERT(magnification != TextureFilterType::Anisotropic && mipLevel != TextureFilterType::Anisotropic);

                if (magnification == TextureFilterType::Point)
                {
                    d3d12Filter = mipLevel == TextureFilterType::Point ? D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT : D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
                }
                else
                {
                    d3d12Filter = mipLevel == TextureFilterType::Point ? D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                }
            }
            else if (minification == TextureFilterType::Anisotropic)
            {
                SPIELER_ASSERT(magnification == TextureFilterType::Anisotropic && mipLevel == TextureFilterType::Anisotropic);

                d3d12Filter = D3D12_FILTER_ANISOTROPIC;
            }

            return d3d12Filter;
        }

    } // namespace _internal

    RenderTargetView::RenderTargetView(Device& device, const Texture2DResource& texture)
    {
        Init(device, texture);
    }

    void RenderTargetView::Init(Device& device, const Texture2DResource& texture)
    {
        SPIELER_ASSERT(texture.GetResource());

        m_Descriptor = device.GetDescriptorManager().AllocateRTV();

        device.GetNativeDevice()->CreateRenderTargetView(
            texture.GetResource().Get(),
            nullptr,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    DepthStencilView::DepthStencilView(Device& device, const Texture2DResource& texture)
    {
        Init(device, texture);
    }

    void DepthStencilView::Init(Device& device, const Texture2DResource& texture)
    {
        SPIELER_ASSERT(texture.GetResource());

        m_Descriptor = device.GetDescriptorManager().AllocateDSV();

        const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc
        {
            .Format = static_cast<DXGI_FORMAT>(texture.GetFormat()),
            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags = D3D12_DSV_FLAG_NONE,
            .Texture2D = D3D12_TEX2D_DSV{ 0 },
        };

        device.GetNativeDevice()->CreateDepthStencilView(
            texture.GetResource().Get(),
            &dsvDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    void SamplerView::Init(Device& device, DescriptorManager& descriptorManager, const SamplerConfig& samplerConfig)
    {
        m_Descriptor = descriptorManager.AllocateSampler();

        const D3D12_SAMPLER_DESC samplerDesc
        {
            .Filter = _internal::ConvertToD3D12TextureFilter(samplerConfig.TextureFilter),
            .AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(samplerConfig.AddressU),
            .AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(samplerConfig.AddressV),
            .AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(samplerConfig.AddressW),
            .MipLODBias = 0.0f,
            .MaxAnisotropy = 1,
            .ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
            .MinLOD = 0.0f,
            .MaxLOD = D3D12_FLOAT32_MAX
        };

        device.GetNativeDevice()->CreateSampler(
            &samplerDesc, 
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    void SamplerView::Bind(Context& context, uint32_t rootParameterIndex) const
    {
        SPIELER_ASSERT(m_Descriptor);

        context.GetNativeCommandList()->SetGraphicsRootDescriptorTable(
            rootParameterIndex,
            D3D12_GPU_DESCRIPTOR_HANDLE{ m_Descriptor.GPU }
        );
    }

#if 0
    void ConstantBufferView::Init(Device& device, const ConstantBuffer& resource)
    {
        SPIELER_ASSERT(resource.m_UploadBuffer);

        m_Descriptor = device.GetDescriptorManager().AllocateCBV();

        const D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc
        {
            .BufferLocation = resource.m_UploadBuffer->GetElementGPUVirtualAddress(resource.m_Index),
            .SizeInBytes = resource.m_UploadBuffer->GetStride()
        };

        device.GetNativeDevice()->CreateConstantBufferView(
            &cbvDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    void ConstantBufferView::Bind(Context& context, uint32_t rootParameterIndex) const
    {
        SPIELER_ASSERT(m_Descriptor);

        context.GetNativeCommandList()->SetGraphicsRootDescriptorTable(
            rootParameterIndex,
            D3D12_GPU_DESCRIPTOR_HANDLE{ m_Descriptor.GPU }
        );
    }
#endif

    ShaderResourceView::ShaderResourceView(Device& device, const Texture2DResource& texture)
    {
        Init(device, texture);
    }

    void ShaderResourceView::Init(Device& device, const Texture2DResource& texture)
    {
        SPIELER_ASSERT(texture.GetResource());

        m_Descriptor = device.GetDescriptorManager().AllocateSRV();

        const D3D12_RESOURCE_DESC texture2DDesc{ texture.GetResource()->GetDesc()};

        const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc
        {
            .Format = texture2DDesc.Format,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = D3D12_TEX2D_SRV
            {
                .MostDetailedMip = 0,
                .MipLevels = texture2DDesc.MipLevels,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0.0f,
            }
        };

        device.GetNativeDevice()->CreateShaderResourceView(
            texture.GetResource().Get(),
            &srvDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    void ShaderResourceView::Bind(Context& context, uint32_t rootParameterIndex) const
    {
        SPIELER_ASSERT(m_Descriptor);

        context.GetNativeCommandList()->SetGraphicsRootDescriptorTable(
            rootParameterIndex,
            D3D12_GPU_DESCRIPTOR_HANDLE{ m_Descriptor.GPU }
        );
    }

} // namespace spieler::renderer