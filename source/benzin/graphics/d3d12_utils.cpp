#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/d3d12_utils.hpp"

namespace benzin
{

    bool HasD3D12ObjectDebugName(ID3D12Object* d3d12Object)
    {
        BenzinAssert(d3d12Object);

        uint32_t bufferSize = 0;
        return SUCCEEDED(d3d12Object->GetPrivateData(WKPDID_D3DDebugObjectName, &bufferSize, nullptr));
    }

    std::string GetD3D12ObjectDebugName(ID3D12Object* d3d12Object)
    {
        static const size_t maxDebugNameSize = 128;

        BenzinAssert(d3d12Object);

        std::string debugName;
        debugName.resize_and_overwrite(maxDebugNameSize, [&](char* data, size_t size)
        {
            auto alignedSize = static_cast<uint32_t>(size);

            BenzinAssert(d3d12Object->GetPrivateData(WKPDID_D3DDebugObjectName, &alignedSize, data));
            return alignedSize;
        });

        return debugName;
    }

    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, const DebugName& debugName)
    {
        if (debugName.IsEmpty())
        {
            return;
        }

        if (!debugName.IsIndexValid())
        {
            SetD3D12ObjectDebugName(d3d12Object, debugName.Chars);
        }
        else
        {
            SetD3D12ObjectDebugName(d3d12Object, debugName.Chars, debugName.Index);
        }
    }

    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, std::string_view debugName)
    {
        BenzinAssert(d3d12Object);

        if (debugName.empty())
        {
            return;
        }

        const bool isRegistered = !HasD3D12ObjectDebugName(d3d12Object);
        BenzinAssert(d3d12Object->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<uint32_t>(debugName.size()), debugName.data()));

        if (isRegistered)
        {
            BenzinTrace("'{}' is registered", debugName);
        }
    }

    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, std::string_view debugName, uint32_t index)
    {
        SetD3D12ObjectDebugName(d3d12Object, fmt::format("{}{}", debugName, index));
    }

    D3D12_HEAP_PROPERTIES GetD3D12HeapProperties(D3D12_HEAP_TYPE d3d12HeapType)
    {
        return D3D12_HEAP_PROPERTIES
        {
            .Type = d3d12HeapType,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            .CreationNodeMask = 1,
            .VisibleNodeMask = 1,
        };
    }

} // namespace benzin
