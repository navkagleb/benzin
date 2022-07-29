#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/context.hpp"

#include "spieler/core/assert.hpp"
#include "spieler/core/common.hpp"

#include "spieler/renderer/resource.hpp"
#include "spieler/renderer/buffer.hpp"
#include "spieler/renderer/device.hpp"
#include "spieler/renderer/pipeline_state.hpp"
#include "spieler/renderer/root_signature.hpp"
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

    Context::Context(Device& device, uint32_t uploadBufferSize)
        : m_Fence{ device }
        , m_UploadBuffer{ device, uploadBufferSize }
    {
        SPIELER_ASSERT(Init(device));
    }

    void Context::SetDescriptorHeap(const DescriptorHeap& descriptorHeap)
    {
        m_CommandList->SetDescriptorHeaps(1, descriptorHeap.GetNative().GetAddressOf());
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

    void Context::WriteToBuffer(BufferResource& buffer, size_t size, const void* data)
    {
        SPIELER_ASSERT(buffer.GetResource());
        SPIELER_ASSERT(size);
        SPIELER_ASSERT(data);

        const uint32_t uploadBufferOffset{ m_UploadBuffer.Allocate(size) };

        memcpy_s(m_UploadBuffer.GetMappedData() + uploadBufferOffset, size, data, size);

        m_CommandList->CopyBufferRegion(buffer.GetResource().Get(), 0, m_UploadBuffer.GetResource().Get(), uploadBufferOffset, size);
    }

    void Context::WriteToTexture(Texture2DResource& texture, std::vector<SubresourceData>& subresources)
    {
        SPIELER_ASSERT(texture.GetResource());
        SPIELER_ASSERT(!subresources.empty());

        const uint32_t firstSubresource{ 0 };
        
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts;
        layouts.resize(subresources.size());

        std::vector<uint32_t> rowCounts;
        rowCounts.resize(subresources.size());

        std::vector<uint64_t> rowSizes; // Aligned by D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT unlike row pitch in subresources
        rowSizes.resize(subresources.size());

        // Init Layouts and allocate memory in UploadBuffer
        {
            uint64_t requiredUploadBufferSize{ 0 };

            {
                ComPtr<ID3D12Device> d3d12Device;
                texture.GetResource()->GetDevice(IID_PPV_ARGS(&d3d12Device));

                const D3D12_RESOURCE_DESC textureDesc{ texture.GetResource()->GetDesc() };

                d3d12Device->GetCopyableFootprints(
                    &textureDesc,
                    firstSubresource,
                    subresources.size(),
                    m_UploadBuffer.Allocate(0, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT),
                    layouts.data(),
                    rowCounts.data(),
                    rowSizes.data(),
                    &requiredUploadBufferSize
                );
            }

            m_UploadBuffer.Allocate(requiredUploadBufferSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        }
        
        // Copying subresources to UploadBuffer
        for (size_t i = 0; i < subresources.size(); ++i)
        {
            SPIELER_ASSERT(rowSizes[i] <= static_cast<uint64_t>(-1));

            std::byte* destinationData{ m_UploadBuffer.GetMappedData() + layouts[i].Offset };

            for (uint32_t z = 0; z < layouts[i].Footprint.Depth; ++z)
            {
                std::byte* destinationSlice{ destinationData + layouts[i].Footprint.RowPitch * static_cast<uint64_t>(rowCounts[i]) * z };
                const std::byte* sourceSlice{ subresources[i].Data + subresources[i].SlicePitch * z };

                for (uint32_t y = 0; y < rowCounts[i]; ++y)
                {
                    std::byte* destinationRow{ destinationSlice + static_cast<uint64_t>(layouts[i].Footprint.RowPitch) * y };
                    const std::byte* sourceRow{ sourceSlice + subresources[i].RowPitch * y };

                    memcpy_s(destinationRow, rowSizes[i], sourceRow, rowSizes[i]);
                }
            }
        }

        // Copy to texture
        for (size_t i = 0; i < subresources.size(); ++i)
        {
            const D3D12_TEXTURE_COPY_LOCATION destination
            {
                .pResource{ texture.GetResource().Get() },
                .Type{ D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX },
                .SubresourceIndex{ static_cast<uint32_t>(i + firstSubresource) }
            };

            const D3D12_TEXTURE_COPY_LOCATION source
            {
                .pResource{ m_UploadBuffer.GetResource().Get() },
                .Type{ D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT },
                .PlacedFootprint{ layouts[i] }
            };

            m_CommandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        }
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