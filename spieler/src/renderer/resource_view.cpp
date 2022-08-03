#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/resource_view.hpp"

#include "spieler/renderer/device.hpp"
#include "spieler/renderer/texture.hpp"
#include "spieler/renderer/sampler.hpp"

#include "platform/dx12/dx12_common.hpp"

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

    // #TODO: SPIELER_ASSERT(resource) to all ctors in views

    VertexBufferView::VertexBufferView(const BufferResource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_View = D3D12_VERTEX_BUFFER_VIEW
        {
            .BufferLocation{ static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(resource.GetGPUVirtualAddress()) },
            .SizeInBytes{ resource.GetConfig().ElementSize * resource.GetConfig().ElementCount },
            .StrideInBytes{ resource.GetConfig().ElementSize }
        };
    }

    IndexBufferView::IndexBufferView(const BufferResource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        GraphicsFormat format;

        switch (resource.GetConfig().ElementSize)
        {
            case 2:
            {
                format = GraphicsFormat::R16UnsignedInt;
                break;
            }
            case 4:
            {
                format = GraphicsFormat::R32UnsignedInt;
                break;
            }
            default:
            {
                SPIELER_ASSERT(false);
                break;
            }
        }

        m_View = D3D12_INDEX_BUFFER_VIEW
        {
            .BufferLocation{ static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(resource.GetGPUVirtualAddress()) },
            .SizeInBytes{ resource.GetConfig().ElementSize * resource.GetConfig().ElementCount },
            .Format{ dx12::Convert(format) }
        };
    }
    
    RenderTargetView::RenderTargetView(Device& device, const Resource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_Descriptor = device.GetDescriptorManager().AllocateRTV();

        device.GetDX12Device()->CreateRenderTargetView(
            resource.GetDX12Resource(),
            nullptr,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    DepthStencilView::DepthStencilView(Device& device, const Resource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_Descriptor = device.GetDescriptorManager().AllocateDSV();

        device.GetDX12Device()->CreateDepthStencilView(
            resource.GetDX12Resource(),
            nullptr,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    SamplerView::SamplerView(Device& device, const SamplerConfig& samplerConfig)
    {
        m_Descriptor = device.GetDescriptorManager().AllocateSampler();

        const D3D12_SAMPLER_DESC desc
        {
            .Filter{ _internal::ConvertToD3D12TextureFilter(samplerConfig.TextureFilter) },
            .AddressU{ static_cast<D3D12_TEXTURE_ADDRESS_MODE>(samplerConfig.AddressU) },
            .AddressV{ static_cast<D3D12_TEXTURE_ADDRESS_MODE>(samplerConfig.AddressV) },
            .AddressW{ static_cast<D3D12_TEXTURE_ADDRESS_MODE>(samplerConfig.AddressW) },
            .MipLODBias{ 0.0f },
            .MaxAnisotropy{ 1 },
            .ComparisonFunc{ D3D12_COMPARISON_FUNC_ALWAYS },
            .MinLOD{ 0.0f },
            .MaxLOD{ D3D12_FLOAT32_MAX }
        };

        device.GetDX12Device()->CreateSampler(
            &desc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    // #TODO: 
#if 0
    ConstantBufferView::ConstantBufferView(Device& device, const Resource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_Descriptor = device.GetDescriptorManager().AllocateCBV();

        const D3D12_CONSTANT_BUFFER_VIEW_DESC desc
        {
            .BufferLocation{ resource.GetElementGPUVirtualAddress(resource.m_Index) },
            .SizeInBytes{ resource.GetConGetStride() }
        };

        device.GetDX12Device()->CreateConstantBufferView(
            &desc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }
#endif

    ShaderResourceView::ShaderResourceView(Device& device, const Resource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_Descriptor = device.GetDescriptorManager().AllocateSRV();

        device.GetDX12Device()->CreateShaderResourceView(
            resource.GetDX12Resource(),
            nullptr,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    UnorderedAccessView::UnorderedAccessView(Device& device, const Resource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_Descriptor = device.GetDescriptorManager().AllocateUAV();

        device.GetDX12Device()->CreateUnorderedAccessView(
            resource.GetDX12Resource(),
            nullptr,
            nullptr,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    //////////////////////////////////////////////////////////////////////////
    /// ViewContainer
    //////////////////////////////////////////////////////////////////////////
    ViewContainer::ViewContainer(Resource& resource)
        : m_Resource{ resource }
    {}

    void ViewContainer::Clear()
    {
        m_Views.clear();
    }

} // namespace spieler::renderer
