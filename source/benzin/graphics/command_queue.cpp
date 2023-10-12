#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/command_queue.hpp"

#include "benzin/graphics/command_list.hpp"
#include "benzin/graphics/descriptor_manager.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{

    // CopyCommandQueue
    CopyCommandQueue::CopyCommandQueue(Device& device)
        : CommandQueue{ device, 1 }
    {}

    CopyCommandList& CopyCommandQueue::GetCommandList(uint32_t uploadBufferSize)
    {
        ResetCommandList(0);
        BenzinAssert(m_CommandList.IsValid());

        if (uploadBufferSize != 0)
        {
            m_CommandList.CreateUploadBuffer(m_Device, uploadBufferSize);
        }

        return m_CommandList;
    }

    void CopyCommandQueue::InitCommandList()
    {
        m_CommandList.ReleaseUploadBuffer();
    }

    // ComputeCommandQueue
    ComputeCommandQueue::ComputeCommandQueue(Device& device)
        : CommandQueue{ device, 1 }
    {}

    void ComputeCommandQueue::InitCommandList()
    {
        ID3D12DescriptorHeap* const d3d12DescriptorHeaps[]
        {
            m_Device.GetDescriptorManager().GetD3D12ResourceDescriptorHeap(),
            m_Device.GetDescriptorManager().GetD3D12SamplerDescriptorHeap()
        };

        ID3D12GraphicsCommandList* d3d12GraphicsCommandList = m_CommandList.GetD3D12GraphicsCommandList();
        d3d12GraphicsCommandList->SetDescriptorHeaps(static_cast<uint32_t>(std::size(d3d12DescriptorHeaps)), d3d12DescriptorHeaps);
        d3d12GraphicsCommandList->SetComputeRootSignature(m_Device.GetD3D12BindlessRootSignature());
    }

    // GraphicsCommandQueue
    GraphicsCommandQueue::GraphicsCommandQueue(Device& device)
        : CommandQueue{ device, config::g_BackBufferCount }
    {}

    void GraphicsCommandQueue::InitCommandList()
    {
        ID3D12DescriptorHeap* const d3d12DescriptorHeaps[]
        {
            m_Device.GetDescriptorManager().GetD3D12ResourceDescriptorHeap(),
            m_Device.GetDescriptorManager().GetD3D12SamplerDescriptorHeap()
        };

        ID3D12GraphicsCommandList* d3d12GraphicsCommandList = m_CommandList.GetD3D12GraphicsCommandList();
        d3d12GraphicsCommandList->SetDescriptorHeaps(static_cast<uint32_t>(std::size(d3d12DescriptorHeaps)), d3d12DescriptorHeaps);
        d3d12GraphicsCommandList->SetComputeRootSignature(m_Device.GetD3D12BindlessRootSignature());
        d3d12GraphicsCommandList->SetGraphicsRootSignature(m_Device.GetD3D12BindlessRootSignature());
    }

} // namespace benzin
