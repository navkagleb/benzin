#pragma once

#include "benzin/core/enum_flags.hpp"

namespace benzin
{

    D3D12_HEAP_PROPERTIES GetD3D12HeapProperties(D3D12_HEAP_TYPE d3d12HeapType);

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

    std::string_view DxgiErrorToString(HRESULT hr);

    void EnableDred();
    std::string GetDredMessages(ID3D12Device* d3d12Device);

    using DxObjectVariant = std::variant<IDXGIObject*, ID3D12Object*>;

    std::string GetDxObjectDebugName(DxObjectVariant dxObjectVariant);
    void SetDxObjectDebugName(DxObjectVariant dxObjectVariant, std::string_view debugName);

    void ReleaseDxObject(DxObjectVariant dxObjectVariant);

} // namespace benzin

#define BenzinSafeDxObjectRelease(unknownPtr) \
    benzin::ReleaseDxObject(unknownPtr); \
    unknownPtr = nullptr
