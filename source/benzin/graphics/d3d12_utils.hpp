#pragma once

#include <benzin/graphics/d3d12_assert.hpp>

namespace benzin
{

    enum class GpuHeapType : uint8_t;

    class Device;

    static constexpr uint32_t g_MaxDebugNameSize = 128;

    D3D12_HEAP_PROPERTIES GetD3D12HeapProperties(D3D12_HEAP_TYPE d3d12HeapType);
    D3D12_HEAP_TYPE ToD3D12HeapType(const Device& device, GpuHeapType gpuHeapType);

    enum class D3D12BreakReasonFlag
    {
        Warning,
        Error,
        Corruption,
    };
    BenzinEnableFlagsForEnum(D3D12BreakReasonFlag);

    void EnableD3D12DebugLayer();
    void EnableD3D12DebugBreakOn(ID3D12Device* d3d12Device, bool isEnabled, EnumFlags<D3D12BreakReasonFlag> flags);
    void ReportLiveD3D12Objects(ID3D12Device* d3d12Device);

    std::string_view DxgiErrorToString(HRESULT hr);

    void EnableDred();
    std::string GetDredMessages(ID3D12Device* d3d12Device);

    template <typename D3DObjectT>
    std::string GetD3DObjectDebugName(D3DObjectT* d3dObject)
    {
        static_assert(std::derived_from<D3DObjectT, ID3D12Object> || std::derived_from<D3DObjectT, IDXGIObject>);

        BenzinAssert(d3dObject != nullptr);

        std::string debugName;
        debugName.resize_and_overwrite(g_MaxDebugNameSize, [&](char* data, size_t size) noexcept -> size_t
        {
            auto alignedSize = (uint32_t)size;

            if (SUCCEEDED(d3dObject->GetPrivateData(WKPDID_D3DDebugObjectName, &alignedSize, data)))
            {
                return alignedSize;
            }

            BenzinWarning("The debug name isn't set! Set debug name before get it!");
            return 0;
        });

        return debugName;
    }

    template <typename D3DObjectT>
    void SetD3DObjectDebugName(D3DObjectT* d3dObject, std::string_view debugName)
    {
        static_assert(std::derived_from<D3DObjectT, ID3D12Object> || std::derived_from<D3DObjectT, IDXGIObject>);

        BenzinAssert(debugName.size() <= g_MaxDebugNameSize);
        if (debugName.empty())
        {
            BenzinWarning("Debug name for resource is empty!");
            return;
        }

        BenzinAssert(d3dObject != nullptr);
        BenzinD3D12Call(d3dObject->SetPrivateData(WKPDID_D3DDebugObjectName, (uint32_t)debugName.size(), debugName.data()));
    }

    template <typename D3DObjectT>
    void SafeReleaseD3DObject(D3DObjectT*& d3dObject, bool isWarningEnabled = true)
    {
        static_assert(std::derived_from<D3DObjectT, ID3D12Object> || std::derived_from<D3DObjectT, IDXGIObject>);

        const auto debugName = GetD3DObjectDebugName(d3dObject);
        const uint32_t referenceCount = d3dObject->Release();

        BenzinWarningIf(isWarningEnabled && referenceCount != 0, "Remaining reference count {}. D3DObject '{}'", referenceCount, debugName);

        d3dObject = nullptr;
    }

}
