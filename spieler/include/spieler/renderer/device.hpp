#pragma once

#include "spieler/renderer/descriptor_manager.hpp"
#include "spieler/renderer/buffer.hpp"
#include "spieler/renderer/texture.hpp"

namespace spieler::renderer
{

    class Device
    {
    public:
        SPIELER_NON_COPYABLE(Device)

    public:
        Device();

    public:
        ID3D12Device* GetDX12Device() const { return m_DX12Device.Get(); }

        DescriptorManager& GetDescriptorManager() { return m_DescriptorManager; }

    public:
        ID3D12Resource* CreateDX12Buffer(const BufferResource::Config& config);

        ID3D12Resource* CreateDX12Texture(const TextureResource::Config& config);
        ID3D12Resource* CreateDX12Texture(const TextureResource::Config& config, const TextureResource::ClearColor& clearColor);
        ID3D12Resource* CreateDX12Texture(const TextureResource::Config& config, const TextureResource::ClearDepthStencil& clearDepthStencil);

    private:
        ID3D12Resource* CreateDX12Resource(const D3D12_HEAP_PROPERTIES* heapProperties, const D3D12_RESOURCE_DESC* resourceDesc, const D3D12_CLEAR_VALUE* clearValueDesc);

    private:
        ComPtr<ID3D12Device> m_DX12Device;

        DescriptorManager m_DescriptorManager;
    };

} // namespace spieler::renderer
