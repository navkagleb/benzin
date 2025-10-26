#pragma once

namespace benzin
{

    class Device;

    struct QueryHeapCreation
    {
        std::string_view m_DebugName;
        D3D12_QUERY_HEAP_TYPE m_D3D12Type = g_MaxEnum<D3D12_QUERY_HEAP_TYPE>;
        uint32_t m_Count = 0;
    };

    class QueryHeap
    {
    public:
        QueryHeap(Device& device, const QueryHeapCreation& creation);
        ~QueryHeap();

        BenzinDefineNonCopyable(QueryHeap);
        BenzinDefineNonMoveable(QueryHeap);

        auto* GetD3D12QueryHeap() const { return m_D3D12QueryHeap; }

        auto GetCount() const { return m_Count; }

    private:
        Device& m_Device;
        ID3D12QueryHeap* m_D3D12QueryHeap = nullptr;
        uint32_t m_Count = 0;
    };

}
