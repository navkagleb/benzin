#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/resource_view_builder.hpp"

#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/api/buffer.hpp"
#include "benzin/graphics/api/texture.hpp"

namespace benzin
{

    namespace
    {

        D3D12_SHADER_RESOURCE_VIEW_DESC FillD3D12ShaderResourceViewDesc(
            TextureResource::Type type,
            DXGI_FORMAT dxgiFormat,
            bool isArray,
            bool isCubeMap,
            uint32_t mostDetailedMipIndex,
            uint32_t mipCount,
            uint32_t firstArrayIndex,
            uint32_t arraySize
        )
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SRVDesc
            {
                .Format{ dxgiFormat },
                .Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING }
            };

            switch (type)
            {
                case TextureResource::Type::Texture2D:
                {
                    if (!isArray)
                    {
                        d3d12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                        d3d12SRVDesc.Texture2D = D3D12_TEX2D_SRV
                        {
                            .MostDetailedMip{ mostDetailedMipIndex },
                            .MipLevels{ mipCount },
                            .PlaneSlice{ 0 },
                            .ResourceMinLODClamp{ 0.0f }
                        };
                    }
                    else if (isCubeMap)
                    {
                        d3d12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                        d3d12SRVDesc.TextureCube = D3D12_TEXCUBE_SRV
                        {
                            .MostDetailedMip{ mostDetailedMipIndex },
                            .MipLevels{ mipCount },
                            .ResourceMinLODClamp{ 0.0f }
                        };
                    }
                    else
                    {
                        d3d12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                        d3d12SRVDesc.Texture2DArray = D3D12_TEX2D_ARRAY_SRV
                        {
                            .MostDetailedMip{ mostDetailedMipIndex },
                            .MipLevels{ mipCount },
                            .FirstArraySlice{ firstArrayIndex },
                            .ArraySize{ arraySize },
                            .PlaneSlice{ 0 },
                            .ResourceMinLODClamp{ 0.0f }
                        };
                    }

                    break;
                }
                default:
                {
                    BENZIN_WARNING("Unsupported TextureResource::Type!");
                    BENZIN_ASSERT(false);

                    break;
                }
            }

            return d3d12SRVDesc;
        }

        D3D12_RENDER_TARGET_VIEW_DESC FillD3D12RenderTargetViewDesc(
            TextureResource::Type type,
            DXGI_FORMAT dxgiFormat,
            bool isArray,
            uint32_t firstArrayIndex,
            uint32_t arraySize
        )
        {
            D3D12_RENDER_TARGET_VIEW_DESC d3d12RTVDesc
            {
                .Format{ dxgiFormat }
            };

            switch (type)
            {
                case TextureResource::Type::Texture2D:
                {
                    if (!isArray)
                    {
                        d3d12RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                        d3d12RTVDesc.Texture2D = D3D12_TEX2D_RTV
                        {
                            .MipSlice{ 0 },
                            .PlaneSlice{ 0 }
                        };
                    }
                    else
                    {
                        d3d12RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                        d3d12RTVDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
                        {
                            .MipSlice{ 0 },
                            .FirstArraySlice{ firstArrayIndex },
                            .ArraySize{ arraySize },
                            .PlaneSlice{ 0 }
                        };
                    }

                    break;
                }
                default:
                {
                    BENZIN_WARNING("Unsupported TextureResource::Type!");
                    BENZIN_ASSERT(false);

                    break;
                }
            }

            return d3d12RTVDesc;
        }

    } // anonymous namespace

	ResourceViewBuilder::ResourceViewBuilder(Device& device)
		: m_Device{ device }
	{}

    Descriptor ResourceViewBuilder::CreateConstantBufferView(const BufferResource& bufferResource)
    {
        ID3D12Resource* d3d12Resource = bufferResource.GetD3D12Resource();
        BENZIN_ASSERT(d3d12Resource);

        const BufferResource::Config config = bufferResource.GetConfig();

        const D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12CBVDesc
        {
            .BufferLocation{ bufferResource.GetGPUVirtualAddress() },
            .SizeInBytes{ config.ElementCount * config.ElementSize }
        };

        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::ShaderResourceView);

        m_Device.GetD3D12Device()->CreateConstantBufferView(
            &d3d12CBVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return descriptor;
    }

    Descriptor ResourceViewBuilder::CreateShaderResourceView(const BufferResource& bufferResource)
    {
        ID3D12Resource* d3d12Resource = bufferResource.GetD3D12Resource();
        BENZIN_ASSERT(d3d12Resource);

        const BufferResource::Config config = bufferResource.GetConfig();

        const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SRVDesc
        {
            .Format{ DXGI_FORMAT_UNKNOWN },
            .ViewDimension{ D3D12_SRV_DIMENSION_BUFFER },
            .Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
            .Buffer
            {
                .FirstElement{ 0 },
                .NumElements{ config.ElementCount },
                .StructureByteStride{ config.ElementSize },
                .Flags{ D3D12_BUFFER_SRV_FLAG_NONE }
            }
        };

        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::ShaderResourceView);

        m_Device.GetD3D12Device()->CreateShaderResourceView(
            d3d12Resource,
            &d3d12SRVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return descriptor;
    }

    Descriptor ResourceViewBuilder::CreateShaderResourceView(const TextureResource& textureResource, const TextureShaderResourceViewConfig& config)
    {
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::ShaderResourceView);
        InitShaderResourceViewForDescriptor(descriptor, textureResource, config);

        return descriptor;
    }

    Descriptor ResourceViewBuilder::CreateUnorderedAccessView(const TextureResource& textureResource)
    {
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::UnorderedAccessView);
        InitUnorderedAccessViewForDescriptor(descriptor, textureResource);

        return descriptor;
    }

    Descriptor ResourceViewBuilder::CreateRenderTargetView(const TextureResource& textureResource, const TextureRenderTargetViewConfig& config)
    {
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::RenderTargetView);
        InitRenderTargetViewForDescriptor(descriptor, textureResource, config);

        return descriptor;
    }

    Descriptor ResourceViewBuilder::CreateDepthStencilView(const TextureResource& textureResource)
    {
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::DepthStencilView);
        InitDepthStencilViewForDescriptor(descriptor, textureResource);

        return descriptor;
    }

    void ResourceViewBuilder::InitShaderResourceViewForDescriptor(const Descriptor& descriptor, const TextureResource& textureResource, const TextureShaderResourceViewConfig& config) const
    {
        ID3D12Resource* d3d12Resource = textureResource.GetD3D12Resource();
        BENZIN_ASSERT(d3d12Resource);

        const TextureResource::Type type = config.Type != TextureResource::Type::Unknown ? config.Type : textureResource.GetConfig().Type;
        BENZIN_ASSERT(type != TextureResource::Type::Unknown);

        const GraphicsFormat format = config.Format != GraphicsFormat::Unknown ? config.Format : textureResource.GetConfig().Format;
        BENZIN_ASSERT(format != GraphicsFormat::Unknown);

        const bool isArray = textureResource.GetConfig().ArraySize != 1;
        const bool isCubeMap = config.IsCubeMap ? true : textureResource.GetConfig().IsCubeMap;

        const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SRVDesc = FillD3D12ShaderResourceViewDesc(
            type,
            static_cast<DXGI_FORMAT>(format),
            isArray,
            isCubeMap,
            config.MostDetailedMipIndex,
            config.MipCount,
            config.FirstArrayIndex,
            config.ArraySize
        );

        m_Device.GetD3D12Device()->CreateShaderResourceView(
            d3d12Resource,
            &d3d12SRVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );
    }

	void ResourceViewBuilder::InitUnorderedAccessViewForDescriptor(const Descriptor& descriptor, const TextureResource& textureResource) const
	{
        using namespace magic_enum::bitwise_operators;
        using Flags = TextureResource::Flags;

		ID3D12Resource* d3d12Resource = textureResource.GetD3D12Resource();
		BENZIN_ASSERT(d3d12Resource);

        BENZIN_ASSERT((textureResource.GetConfig().Flags & Flags::BindAsUnorderedAccess) != Flags::None);

		ID3D12Resource* d3d12CounterResource = nullptr;
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* d3d12UAVDesc = nullptr;

		m_Device.GetD3D12Device()->CreateUnorderedAccessView(
			d3d12Resource,
			d3d12CounterResource,
			d3d12UAVDesc,
			D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
		);
	}

    void ResourceViewBuilder::InitRenderTargetViewForDescriptor(const Descriptor& descriptor, const TextureResource& textureResource, const TextureRenderTargetViewConfig& config) const
    {
        using namespace magic_enum::bitwise_operators;
        using Flags = TextureResource::Flags;

        ID3D12Resource* d3d12Resource = textureResource.GetD3D12Resource();
        BENZIN_ASSERT(d3d12Resource);

        BENZIN_ASSERT((textureResource.GetConfig().Flags & Flags::BindAsRenderTarget) != Flags::None);

        const GraphicsFormat format = config.Format != GraphicsFormat::Unknown ? config.Format : textureResource.GetConfig().Format;
        BENZIN_ASSERT(format != GraphicsFormat::Unknown);

        const bool isArray = textureResource.GetConfig().ArraySize != 1;

        const D3D12_RENDER_TARGET_VIEW_DESC d3d12RTVDesc = FillD3D12RenderTargetViewDesc(
            textureResource.GetConfig().Type,
            static_cast<DXGI_FORMAT>(format),
            isArray,
            config.FirstArrayIndex,
            config.ArraySize
        );

        m_Device.GetD3D12Device()->CreateRenderTargetView(
            d3d12Resource,
            &d3d12RTVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );
    }

	void ResourceViewBuilder::InitDepthStencilViewForDescriptor(const Descriptor& descriptor, const TextureResource& textureResource) const
	{
        using namespace magic_enum::bitwise_operators;
        using Flags = TextureResource::Flags;

        ID3D12Resource* d3d12Resource = textureResource.GetD3D12Resource();
        BENZIN_ASSERT(d3d12Resource);

        BENZIN_ASSERT((textureResource.GetConfig().Flags & Flags::BindAsDepthStencil) != Flags::None);

        const D3D12_DEPTH_STENCIL_VIEW_DESC* d3d12DSVDesc = nullptr;

        m_Device.GetD3D12Device()->CreateDepthStencilView(
            d3d12Resource,
            d3d12DSVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );
	}

} // namespace benzin
