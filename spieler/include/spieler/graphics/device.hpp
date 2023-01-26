#pragma once

#include "spieler/graphics/buffer.hpp"
#include "spieler/graphics/texture.hpp"
namespace spieler
{

    class Device
    {
    public:
        SPIELER_NON_COPYABLE(Device)
        SPIELER_NON_MOVEABLE(Device)
        SPIELER_NAME_D3D12_OBJECT(m_D3D12Device)

    public:
        Device();
        ~Device();

    public:
        ID3D12Device* GetD3D12Device() const { return m_D3D12Device; }

        DescriptorManager& GetDescriptorManager() { return *m_DescriptorManager; }
        ResourceLoader& GetResourceLoader() { return *m_ResourceLoader; }
        ResourceViewBuilder& GetResourceViewBuilder() { return *m_ResourceViewBuilder; }

    public:
        std::shared_ptr<BufferResource> CreateBufferResource(const BufferResource::Config& config) const;

        std::shared_ptr<TextureResource> RegisterTextureResource(ID3D12Resource* d3d12Resource) const;
        std::shared_ptr<TextureResource> CreateTextureResource(const TextureResource::Config& config) const;
        std::shared_ptr<TextureResource> CreateTextureResource(const TextureResource::Config& config, const TextureResource::ClearColor& clearColor) const;
        std::shared_ptr<TextureResource> CreateTextureResource(const TextureResource::Config& config, const TextureResource::ClearDepthStencil& clearDepthStencil) const;

    private:
        ID3D12Resource* CreateD3D12CommittedResource(
            D3D12_HEAP_TYPE d3d12HeapType,
            const D3D12_RESOURCE_DESC* d3d12ResourceDesc,
            const D3D12_CLEAR_VALUE* d3d12ClearValueDesc
        ) const;

    private:
        ID3D12Device* m_D3D12Device{ nullptr };

        DescriptorManager* m_DescriptorManager{ nullptr };
        ResourceLoader* m_ResourceLoader{ nullptr };
        ResourceViewBuilder* m_ResourceViewBuilder{ nullptr };

        std::function<void(Resource*)> m_ResourceReleaser;
    };

} // namespace spieler
