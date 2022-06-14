#pragma once

#include "spieler/renderer/descriptor_manager.hpp"
#include "spieler/renderer/buffer.hpp"
#include "spieler/renderer/texture.hpp"

namespace spieler::renderer
{

    class Resource;

    struct BufferConfig;

    struct Texture2DConfig;
    struct TextureClearValue;
    struct DepthStencilClearValue;

    class Device
    {
    public:
        SPIELER_NON_COPYABLE(Device)

    public:
        Device();

    public:
        ComPtr<ID3D12Device>& GetNativeDevice() { return m_Device; }
        const ComPtr<ID3D12Device>& GetNativeDevice() const { return m_Device; }

        DescriptorManager& GetDescriptorManager() { return m_DescriptorManager; }

    public:
        bool CreateDefaultBuffer(const BufferConfig& bufferConfig, Resource& resource);
        bool CreateUploadBuffer(const BufferConfig& bufferConfig, Resource& resource);

        bool CreateTexture(const Texture2DConfig& texture2DConfig, Resource& resource);
        bool CreateTexture(const Texture2DConfig& texture2DConfig, const TextureClearValue& textureClearValue, Resource& resource);
        bool CreateTexture(const Texture2DConfig& texture2DConfig, const DepthStencilClearValue& depthStencilClearValue, Resource& resource);

        std::shared_ptr<BufferResource> CreateBuffer(const BufferConfig& bufferConfig, BufferFlags bufferFlags);

    private:
        bool CreateResource(const D3D12_HEAP_PROPERTIES* heapDesc, const D3D12_RESOURCE_DESC* resourceDesc, const D3D12_CLEAR_VALUE* clearValueDesc, ComPtr<ID3D12Resource>& resource);

    private:
        ComPtr<ID3D12Device> m_Device;

        DescriptorManager m_DescriptorManager;
    };

} // namespace spieler::renderer