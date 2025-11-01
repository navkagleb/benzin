#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/d3d12_utils.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>

namespace benzin
{

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

    D3D12_HEAP_TYPE ToD3D12HeapType(const Device& device, GpuHeapType gpuHeapType)
    {
        if (gpuHeapType == GpuHeapType::Upload)
            return D3D12_HEAP_TYPE_UPLOAD;

        if (gpuHeapType == GpuHeapType::Readback)
            return D3D12_HEAP_TYPE_READBACK;

        if (gpuHeapType == GpuHeapType::GpuUpload)
            return device.GetCaps().m_IsGpuUploadHeapsSupported ? D3D12_HEAP_TYPE_GPU_UPLOAD : D3D12_HEAP_TYPE_UPLOAD;

        return D3D12_HEAP_TYPE_DEFAULT;
    }

}
