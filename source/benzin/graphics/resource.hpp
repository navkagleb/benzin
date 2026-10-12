#pragma once

#include <benzin/graphics/descriptor_manager.hpp>

namespace benzin
{

    struct SubResourceData
    {
        const std::byte* m_Data = nullptr;
        uint64_t m_RowPitchInBytes = 0;
        uint64_t m_SlicePitchInBytes = 0;
    };

    class Resource
    {
    protected:
        explicit Resource(Device& device);
        virtual ~Resource();

        BenzinDefineNonCopyable(Resource);
        BenzinDefineNonMoveable(Resource);

    public:
        auto* GetD3D12Resource() const { return m_D3D12Resource; }

        auto GetD3D12State() const { return m_D3D12CurrentState; }
        void SetD3D12State(D3D12_RESOURCE_STATES d3d12State) const { m_D3D12CurrentState = d3d12State; }

        virtual uint64_t GetSizeInBytes() const = 0; // TODO: Rename to GetCopyableSizeInBytes

    protected:
        template <typename T>
        Descriptor& GetViewDescriptor(const T& viewDesc) const
        {
            const size_t hash = std::hash<T>{}(viewDesc);
            return m_ViewDescriptors[hash];
        }

        const Descriptor& TryGetViewDescriptor(size_t hash, std::function<Descriptor()>&& createDescriptorCallback) const;

        Device& m_Device;

        ID3D12Resource* m_D3D12Resource = nullptr;
        mutable D3D12_RESOURCE_STATES m_D3D12CurrentState = D3D12_RESOURCE_STATE_COMMON;

    private:
        mutable std::unordered_map<size_t, Descriptor> m_ViewDescriptors;
    };

}
