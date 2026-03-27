#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/query_heap.hpp>

#include <benzin/graphics/d3d12_debug.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>

namespace benzin
{

    QueryHeap::QueryHeap(Device& device, const QueryHeapCreation& creation)
        : m_Device{ device }
    {
        BenzinAssert(!IsMaxEnum(creation.m_D3D12Type));
        BenzinAssert(creation.m_Count != 0);

        D3D12_QUERY_HEAP_DESC d3d12QueryHeapDesc = {};
        d3d12QueryHeapDesc.Type = creation.m_D3D12Type;
        d3d12QueryHeapDesc.Count = creation.m_Count;
        d3d12QueryHeapDesc.NodeMask = 0;

        BenzinD3D12Call(m_Device.GetD3D12Device()->CreateQueryHeap(&d3d12QueryHeapDesc, IID_PPV_ARGS(&m_D3D12QueryHeap)));
        BenzinEnsure(m_D3D12QueryHeap != nullptr);

        SetD3DObjectDebugName(m_D3D12QueryHeap, creation.m_DebugName);

        m_D3D12Type = creation.m_D3D12Type;
        m_Count = creation.m_Count;
    }

    QueryHeap::~QueryHeap()
    {
        m_Device.DeferredRelease(m_D3D12QueryHeap);
        m_D3D12QueryHeap = nullptr;
    }

}
