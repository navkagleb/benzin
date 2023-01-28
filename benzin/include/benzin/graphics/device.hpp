#pragma once

#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/texture.hpp"
namespace benzin
{

    class Device
    {
    public:
        BENZIN_NON_COPYABLE(Device)
        BENZIN_NON_MOVEABLE(Device)
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12Device, "Device")

    public:
        Device();
        ~Device();

    public:
        ID3D12Device* GetD3D12Device() const { return m_D3D12Device; }

        DescriptorManager& GetDescriptorManager() { return *m_DescriptorManager; }
        ResourceLoader& GetResourceLoader() { return *m_ResourceLoader; }
        ResourceViewBuilder& GetResourceViewBuilder() { return *m_ResourceViewBuilder; }

    public:
        std::shared_ptr<BufferResource> CreateBufferResource(const BufferResource::Config& config, const std::string& debugName = {}) const;

        std::shared_ptr<TextureResource> RegisterTextureResource(ID3D12Resource* d3d12Resource, const std::string& debugName = {}) const;
        std::shared_ptr<TextureResource> CreateTextureResource(const TextureResource::Config& config, const std::string& debugName = {}) const;
        std::shared_ptr<TextureResource> CreateTextureResource(const TextureResource::Config& config, const TextureResource::ClearColor& clearColor, const std::string& debugName = {}) const;
        std::shared_ptr<TextureResource> CreateTextureResource(const TextureResource::Config& config, const TextureResource::ClearDepthStencil& clearDepthStencil, const std::string& debugName = {}) const;

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

} // namespace benzin
