#pragma once

#include "spieler/renderer/common.hpp"
#include "spieler/renderer/descriptor_manager.hpp"

namespace spieler::renderer
{

    struct SubresourceData
    {
        const std::byte* Data{ nullptr };
        uint64_t RowPitch{ 0 };
        uint64_t SlicePitch{ 0 };
    };

    using GPUVirtualAddress = uint64_t;

    class Resource
    {
    public:
        friend class Device;

    public:
        Resource() = default;
        Resource(ID3D12Resource* dx12Resource);
        virtual ~Resource();

    public:
        ID3D12Resource* GetDX12Resource() const { return m_DX12Resource; }

    public:
        GPUVirtualAddress GetGPUVirtualAddress() const;
        
    protected:
        ID3D12Resource* m_DX12Resource{ nullptr };
    };

    template <typename Descriptor>
    class DescriptorView
    {
    public:
        const Descriptor& GetDescriptor() const { return m_Descriptor; }

    protected:
        Descriptor m_Descriptor;
    };

} // namespace spieler::renderer
