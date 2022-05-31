#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/context.hpp"

#include "spieler/core/assert.hpp"
#include "spieler/core/common.hpp"

#include "spieler/renderer/resource.hpp"
#include "spieler/renderer/device.hpp"
#include "spieler/renderer/pipeline_state.hpp"
#include "spieler/renderer/upload_buffer.hpp"
#include "spieler/renderer/texture.hpp"

namespace spieler::renderer
{

    namespace _internal
    {

        static D3D12_RESOURCE_BARRIER ConvertToD3D12ResourceBarrier(const TransitionResourceBarrier& barrier)
        {
            const D3D12_RESOURCE_TRANSITION_BARRIER d3d12TransitionBarrier
            {
                .pResource = static_cast<ID3D12Resource*>(*barrier.Resource),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = static_cast<D3D12_RESOURCE_STATES>(barrier.From),
                .StateAfter = static_cast<D3D12_RESOURCE_STATES>(barrier.To)
            };

            return D3D12_RESOURCE_BARRIER
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition = d3d12TransitionBarrier
            };
        }

    } // namespace _internal

    Context::Context(Device& device)
        : m_Fence(device)
    {
        SPIELER_ASSERT(Init(device));
    }

    void Context::SetViewport(const Viewport& viewport)
    {
        m_CommandList->RSSetViewports(1, reinterpret_cast<const D3D12_VIEWPORT*>(&viewport));
    }

    void Context::SetScissorRect(const ScissorRect& scissorRect)
    {
        const ::RECT d3d12Rect
        {
            .left = static_cast<LONG>(scissorRect.X),
            .top = static_cast<LONG>(scissorRect.Y),
            .right = static_cast<LONG>(scissorRect.X + scissorRect.Width),
            .bottom = static_cast<LONG>(scissorRect.Y + scissorRect.Height),
        };

        m_CommandList->RSSetScissorRects(1, &d3d12Rect);
    }

    void Context::SetResourceBarrier(const TransitionResourceBarrier& barrier)
    {
        const D3D12_RESOURCE_BARRIER d3d12Barrier{ _internal::ConvertToD3D12ResourceBarrier(barrier) };

        m_CommandList->ResourceBarrier(1, &d3d12Barrier);
    }

    void Context::SetPrimitiveTopology(PrimitiveTopology primitiveTopology)
    {
        m_CommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(primitiveTopology));
    }

    void Context::SetStencilReferenceValue(uint8_t referenceValue)
    {
        m_CommandList->OMSetStencilRef(referenceValue);
    }

    void Context::CopyBuffer(const void* data, uint64_t size, UploadBuffer& from, Resource& to)
    {
        std::vector<D3D12_SUBRESOURCE_DATA> subresources
        {
            D3D12_SUBRESOURCE_DATA
            {
                .pData = data,
                .RowPitch = static_cast<::LONG_PTR>(size),
                .SlicePitch = static_cast<::LONG_PTR>(size)
            }
        };

        Copy(subresources, 0, from, to);
    }

    void Context::Copy(std::vector<D3D12_SUBRESOURCE_DATA>& subresources, uint32_t alingment, UploadBuffer& from, Resource& to)
    {
        // TODO: Set barrier to resources
#if 0
        SetResourceBarrier(TransitionResourceBarrier
        {
            .Resource = &to,
            .From = ResourceState::Present,
            .To = ResourceState::CopyDestination
        });
#endif

        const uint32_t uploadBufferSize{ static_cast<uint32_t>(::GetRequiredIntermediateSize(to.GetResource().Get(), 0, static_cast<uint32_t>(subresources.size()))) };
        const uint32_t offset{ from.Allocate(uploadBufferSize, alingment) };
        const uint32_t firstSubresource{ 0 };

        UpdateSubresources(
            m_CommandList.Get(),
            to.GetResource().Get(),
            from.GetResource().Get(),
            offset,
            firstSubresource,
            static_cast<uint32_t>(subresources.size()),
            subresources.data()
        );

        // TODO: Set barrier to resources
#if 0
        SetResourceBarrier(TransitionResourceBarrier
        {
            .Resource = &to,
            .From = ResourceState::CopyDestination,
            .To = ResourceState::GenericRead
        });
#endif
    }

    bool Context::CloseCommandList()
    {
        SPIELER_RETURN_IF_FAILED(m_CommandList->Close());

        return true;
    }

    bool Context::ExecuteCommandList(bool isNeedToFlushCommandQueue)
    {
        SPIELER_RETURN_IF_FAILED(CloseCommandList());

        m_CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(m_CommandList.GetAddressOf()));

        if (isNeedToFlushCommandQueue)
        {
            SPIELER_RETURN_IF_FAILED(FlushCommandQueue());
        }

        return true;
    }

    bool Context::FlushCommandQueue()
    {
        m_Fence.Increment();

        SPIELER_RETURN_IF_FAILED(m_Fence.Signal(*this));
        SPIELER_RETURN_IF_FAILED(m_Fence.WaitForGPU());

        return true;
    }

    bool Context::ResetCommandList(const PipelineState* pso)
    {
        ID3D12PipelineState* d3d12PSO{ pso ? static_cast<ID3D12PipelineState*>(*pso) : nullptr };

        SPIELER_RETURN_IF_FAILED(m_CommandList->Reset(m_CommandAllocator.Get(), d3d12PSO));

        return true;
    }

    bool Context::ResetCommandAllocator()
    {
        SPIELER_RETURN_IF_FAILED(m_CommandAllocator->Reset());

        return true;
    }

    bool Context::Init(Device& device)
    {
        return InitCommandAllocator(device) && InitCommandList(device) && InitCommandQueue(device);
    }

    bool Context::InitCommandAllocator(Device& device)
    {
        SPIELER_RETURN_IF_FAILED(device.GetNativeDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &m_CommandAllocator));

        return true;
    }

    bool Context::InitCommandList(Device& device)
    {
        SPIELER_RETURN_IF_FAILED(device.GetNativeDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, __uuidof(ID3D12CommandList), &m_CommandList));
        SPIELER_RETURN_IF_FAILED(m_CommandList->Close());

        return true;
    }

    bool Context::InitCommandQueue(Device& device)
    {
        const D3D12_COMMAND_QUEUE_DESC desc
        {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = 0,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0
        };

        SPIELER_RETURN_IF_FAILED(device.GetNativeDevice()->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), &m_CommandQueue));

        return true;
    }

} // namespace spieler::renderer