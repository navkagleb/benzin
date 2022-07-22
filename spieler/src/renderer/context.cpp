#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/context.hpp"

#include "spieler/core/assert.hpp"
#include "spieler/core/common.hpp"

#include "spieler/renderer/resource.hpp"
#include "spieler/renderer/device.hpp"
#include "spieler/renderer/pipeline_state.hpp"
#include "spieler/renderer/root_signature.hpp"
#include "spieler/renderer/upload_buffer.hpp"
#include "spieler/renderer/texture.hpp"

#include "spieler/renderer/vertex_buffer.hpp"
#include "spieler/renderer/index_buffer.hpp"

#include "spieler/renderer/resource_view.hpp"

#include "platform/dx12/dx12_common.hpp"

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

    void Context::IASetVertexBuffer(const VertexBufferView* vertexBufferView)
    {
        m_CommandList->IASetVertexBuffers(0, 1, reinterpret_cast<const D3D12_VERTEX_BUFFER_VIEW*>(vertexBufferView));
    }

    void Context::IASetIndexBuffer(const IndexBufferView* indexBufferView)
    {
        m_CommandList->IASetIndexBuffer(reinterpret_cast<const D3D12_INDEX_BUFFER_VIEW*>(indexBufferView));
    }

    void Context::IASetPrimitiveTopology(PrimitiveTopology primitiveTopology)
    {
        SPIELER_ASSERT(primitiveTopology != PrimitiveTopology::Unknown);

        m_CommandList->IASetPrimitiveTopology(dx12::Convert(primitiveTopology));
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

    void Context::SetPipelineState(const PipelineState& pso)
    {
        m_CommandList->SetPipelineState(static_cast<ID3D12PipelineState*>(pso));
    }

    void Context::SetGraphicsRootSignature(const RootSignature& rootSignature)
    {
        m_CommandList->SetGraphicsRootSignature(static_cast<ID3D12RootSignature*>(rootSignature));
    }

    void Context::SetComputeRootSignature(const RootSignature& rootSignature)
    {
        m_CommandList->SetComputeRootSignature(static_cast<ID3D12RootSignature*>(rootSignature));
    }

    void Context::SetRenderTarget(const RenderTargetView& rtv)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{ rtv.GetDescriptor().CPU };

        m_CommandList->OMSetRenderTargets(1, &rtvDescriptorHandle, true, nullptr);
    }

    void Context::SetRenderTarget(const RenderTargetView& rtv, const DepthStencilView& dsv)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{ rtv.GetDescriptor().CPU };
        const D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle{ dsv.GetDescriptor().CPU };

        m_CommandList->OMSetRenderTargets(1, &rtvDescriptorHandle, true, &dsvDescriptorHandle);
    }

    void Context::ClearRenderTarget(const RenderTargetView& rtv, const DirectX::XMFLOAT4& color)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{ rtv.GetDescriptor().CPU };

        m_CommandList->ClearRenderTargetView(rtvDescriptorHandle, reinterpret_cast<const float*>(&color), 0, nullptr);
    }

    void Context::ClearDepthStencil(const DepthStencilView& dsv, float depth, uint8_t stencil)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle{ dsv.GetDescriptor().CPU };

        m_CommandList->ClearDepthStencilView(
            dsvDescriptorHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            depth,
            stencil,
            0,
            nullptr
        );
    }

    void Context::SetResourceBarrier(const TransitionResourceBarrier& barrier)
    {
        SPIELER_ASSERT(barrier.Resource);

        const D3D12_RESOURCE_BARRIER d3d12Barrier{ _internal::ConvertToD3D12ResourceBarrier(barrier) };

        m_CommandList->ResourceBarrier(1, &d3d12Barrier);
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