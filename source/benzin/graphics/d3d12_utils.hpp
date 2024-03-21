#pragma once

#include "benzin/core/enum_flags.hpp"

namespace benzin
{

#if BENZIN_IS_DEBUG_BUILD
    enum class D3D12BreakReasonFlag
    {
        Warning,
        Error,
        Corruption,
    };
    BenzinEnableFlagsForEnum(D3D12BreakReasonFlag);

    void EnableD3D12DebugLayer();
    void EnableD3D12DebugBreakOn(ID3D12Device* d3d12Device, bool isEnabled, D3D12BreakReasonFlags flags);
    void ReportLiveD3D12Objects(ID3D12Device* d3d12Device);
#endif

    std::string_view DxgiErrorToString(HRESULT hr);

    void EnableDred();
    std::string GetDredMessages(ID3D12Device* d3d12Device);

    bool HasD3D12ObjectDebugName(ID3D12Object* d3d12Object);
    std::string GetD3D12ObjectDebugName(ID3D12Object* d3d12Object);
    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, std::string_view debugName);
    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, std::string_view debugName, uint32_t index);

    D3D12_HEAP_PROPERTIES GetD3D12HeapProperties(D3D12_HEAP_TYPE d3d12HeapType);

    template <std::derived_from<IUnknown> T>
    void SafeUnknownRelease(T*& unknown)
    {
        if (!unknown)
        {
            return;
        }

        const uint32_t referenceCount = unknown->Release();
        // BenzinAssert(referenceCount == 0);
        unknown = nullptr;
    }

} // namespace benzin
