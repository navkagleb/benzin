#pragma once

namespace benzin
{

    class Device;

    enum class QueryHeapType : uint8_t
    {
        // Ref: https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing
        // D3D12_COMMAND_LIST_TYPE_DIRECT and D3D12_COMMAND_LIST_TYPE_COMPUTE always support timestamps
        Timestamp = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
    };

    struct QueryHeapCreation
    {
        std::string_view DebugName;
        QueryHeapType Type = g_InvalidEnum<QueryHeapType>;
        uint32_t Count = 0;
    };

    class QueryHeap
    {
    public:
        QueryHeap(Device& device, const QueryHeapCreation& creation);
        ~QueryHeap();

        BenzinDefineNonCopyable(QueryHeap);
        BenzinDefineNonMoveable(QueryHeap);

        auto* GetD3D12QueryHeap() const { return m_D3D12QueryHeap; }
        auto& GetCount() const { return m_Count; }

    private:
        Device& m_Device;

        ID3D12QueryHeap* m_D3D12QueryHeap = nullptr;
        uint32_t m_Count = 0;
    };

}
