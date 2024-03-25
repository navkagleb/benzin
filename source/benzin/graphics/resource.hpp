#pragma once

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    struct SubResourceData
    {
        const std::byte* Data = nullptr;

        size_t RowPitch = 0;
        size_t SlicePitch = 0;
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

        auto GetCurrentState() const { return m_CurrentState; }
        void SetCurrentState(ResourceState resourceState) { m_CurrentState = resourceState; }

        uint32_t GetAllocationSizeInBytes() const;

        virtual uint32_t GetSizeInBytes() const = 0;

    protected:
        template <typename T>
        Descriptor& GetViewDescriptor(const T& viewDesc) const
        {
            const size_t hash = std::hash<T>{}(viewDesc);
            return m_ViewDescriptors[hash];
        }

    protected:
        Device& m_Device;

        ID3D12Resource* m_D3D12Resource = nullptr;
        ResourceState m_CurrentState = ResourceState::Common;

    private:
        mutable std::unordered_map<size_t, Descriptor> m_ViewDescriptors;
    };

} // namespace benzin
