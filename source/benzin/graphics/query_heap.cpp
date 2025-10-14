#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/query_heap.hpp>

#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/d3d12_assert.hpp>

namespace benzin
{

    QueryHeap::QueryHeap(Device& device, const QueryHeapCreation& creation)
        : m_Device{ device }
    {
        BenzinAssert(IsGoodEnum(creation.Type));
        BenzinAssert(creation.Count != 0);

        const D3D12_QUERY_HEAP_DESC d3d12QueryHeapDesc
        {
            .Type = (D3D12_QUERY_HEAP_TYPE)creation.Type,
            .Count = creation.Count,
            .NodeMask = 0,
        };

        BenzinD3D12Call(m_Device.GetD3D12Device()->CreateQueryHeap(&d3d12QueryHeapDesc, IID_PPV_ARGS(&m_D3D12QueryHeap)));
        BenzinEnsure(m_D3D12QueryHeap != nullptr);

        SetD3DObjectDebugName(m_D3D12QueryHeap, creation.DebugName);

        m_Count = creation.Count;
    }

    QueryHeap::~QueryHeap()
    {
        m_Device.DeferredRelease(m_D3D12QueryHeap);
    }

}
