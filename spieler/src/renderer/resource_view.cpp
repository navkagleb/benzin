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

        const D3D12_RESOURCE_DESC dx12ResourceDesc{ resource.GetDX12Resource()->GetDesc() };

        D3D12_SHADER_RESOURCE_VIEW_DESC dx12SRVDesc
        {
            .Format{ dx12ResourceDesc.Format },
            .Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING }
        };

        if (dx12ResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            const auto& bufferResource{ static_cast<const BufferResource&>(resource) };

            dx12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            dx12SRVDesc.Buffer = D3D12_BUFFER_SRV
            {
                .FirstElement{ 0 },
                .NumElements{ bufferResource.GetConfig().ElementCount },
                .StructureByteStride{ bufferResource.GetConfig().ElementSize },
                .Flags{ D3D12_BUFFER_SRV_FLAG_NONE }
            };
        }
        else
        {
            const auto& textureResource{ static_cast<const TextureResource&>(resource) };

            switch (textureResource.GetConfig().Type)
            {
                case TextureResource::Type::_2D:
                {
                    dx12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    dx12SRVDesc.Texture2D = D3D12_TEX2D_SRV
                    {
                        .MostDetailedMip{ 0 },
                        .MipLevels{ dx12ResourceDesc.MipLevels },
                        .PlaneSlice{ 0 },
                        .ResourceMinLODClamp{ 0.0f }
                    };

                    break;
                }
                case TextureResource::Type::CubeMap:
                {
                    dx12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                    dx12SRVDesc.TextureCube = D3D12_TEXCUBE_SRV
                    {
                        .MostDetailedMip{ 0 },
                        .MipLevels{ dx12ResourceDesc.MipLevels },
                        .ResourceMinLODClamp{ 0.0f }
                    };

                    break;
                }
                default:
                {
                    SPIELER_ASSERT(false);
                }
            }
        }

        device.GetDX12Device()->CreateShaderResourceView(
            resource.GetDX12Resource(),
            &dx12SRVDesc,
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
