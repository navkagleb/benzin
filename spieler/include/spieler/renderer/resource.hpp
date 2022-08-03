#pragma once

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
        Resource(ComPtr<ID3D12Resource>&& dx12Resource);

    public:
        ID3D12Resource* GetDX12Resource() const { return m_DX12Resource.Get(); }

    public:
        GPUVirtualAddress GetGPUVirtualAddress() const { return static_cast<GPUVirtualAddress>(m_DX12Resource->GetGPUVirtualAddress()); }
        
    protected:
        ComPtr<ID3D12Resource> m_DX12Resource;
    };

} // namespace spieler::renderer
