#pragma once

namespace benzin
{

    struct DebugLayerParams
    {
        bool IsGPUBasedValidationEnabled = true;
        bool IsSynchronizedCommandQueueValidationEnabled = true;
    };

    void EnableD3D12DebugLayer(const DebugLayerParams& params);
    void EnableDRED();

    void OnD3D12DeviceRemoved(PVOID context, BOOLEAN);

    void BreakOnD3D12Error(ID3D12Device* d3d12Device, bool isBreak);
    void ReportLiveD3D12Objects(ID3D12Device* d3d12Device);

    struct DebugName
    {
        static const uint32_t s_InvalidIndex = std::numeric_limits<uint32_t>::max();

        std::string_view Chars;
        uint32_t Index = s_InvalidIndex;

        bool IsEmpty() const { return Chars.empty(); }
        bool IsIndexValid() const { return Index != s_InvalidIndex; }
    };

    bool HasD3D12ObjectDebugName(ID3D12Object* d3d12Object);
    std::string GetD3D12ObjectDebugName(ID3D12Object* d3d12Object);

    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, const DebugName& debugName);
    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, std::string_view debugName);
    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, std::string_view debugName, uint32_t index);

    D3D12_HEAP_PROPERTIES GetD3D12HeapProperties(D3D12_HEAP_TYPE d3d12HeapType);

    void SafeUnknownRelease(std::derived_from<IUnknown> auto*& unknown)
    {
        if (!unknown)
        {
            return;
        }

        const uint32_t referenceCount = unknown->Release();
        //BenzinAssert(referenceCount == 0);
        unknown = nullptr;
    }

} // namespace benzin
