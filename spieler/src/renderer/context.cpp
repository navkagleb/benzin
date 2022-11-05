#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/context.hpp"

#include "spieler/core/common.hpp"

#include "spieler/renderer/resource.hpp"
#include "spieler/renderer/buffer.hpp"
#include "spieler/renderer/texture.hpp"
#include "spieler/renderer/device.hpp"
#include "spieler/renderer/pipeline_state.hpp"
#include "spieler/renderer/root_signature.hpp"
#include "spieler/renderer/utils.hpp"
#include "spieler/renderer/mapped_data.hpp"

#include "platform/dx12/dx12_common.hpp"

namespace spieler::renderer
{

    namespace _internal
    {

        static D3D12_RESOURCE_BARRIER ConvertToD3D12ResourceBarrier(const TransitionResourceBarrier& barrier)
        {
            return D3D12_RESOURCE_BARRIER
            {
                .Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
                .Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
                .Transition
                {
                    .pResource{ barrier.Resource->GetDX12Resource() },
                    .Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
                    .StateBefore{ static_cast<D3D12_RESOURCE_STATES>(barrier.From) },
                    .StateAfter{ static_cast<D3D12_RESOURCE_STATES>(barrier.To) }
                }
            };
        }

    } // namespace _internal

    //////////////////////////////////////////////////////////////////////////
    /// Context::CommandListScope
    //////////////////////////////////////////////////////////////////////////
    Context::CommandListScope::CommandListScope(Context& context, bool isNeedToFlushCommandQueue)
        : m_Context{ context }
        , m_IsNeedToFlushCommandQueue{ isNeedToFlushCommandQueue }
    {
        m_Context.ResetCommandList();
    }

    Context::CommandListScope::~CommandListScope()
    {
        m_Context.ExecuteCommandList(m_IsNeedToFlushCommandQueue);
    }

    //////////////////////////////////////////////////////////////////////////
    /// Context::UploadBuffer
    //////////////////////////////////////////////////////////////////////////
    uint64_t Context::UploadBuffer::Allocate(uint64_t size, uint64_t alignment)
    {
        const uint64_t offset{ alignment == 0 ? Offset : utils::Align(Offset, alignment) };

        SPIELER_ASSERT(offset + size <= Resource->GetConfig().ElementCount);
        Offset = offset + size;

        return offset;
    }

    //////////////////////////////////////////////////////////////////////////
    /// Context
    //////////////////////////////////////////////////////////////////////////
    Context::Context(Device& device, uint64_t uploadBufferSize)
        : m_Fence{ device }
    {
        // Init m_UploadBuffer
        {
            const BufferResource::Config config
            {
                .ElementSize{ sizeof(std::byte) },
                .ElementCount{ static_cast<uint32_t>(uploadBufferSize) },
                .Flags{ BufferResource::Flags::Dynamic }
            };

            m_UploadBuffer.Resource = BufferResource::Create(device, config);
        }

        SPIELER_ASSERT(Init(device));
    }

    void Context::SetDescriptorHeap(const DescriptorHeap& descriptorHeap)
    {
        ID3D12DescriptorHeap* dx12DescriptorHeap{ descriptorHeap.GetDX12DescriptorHeap() };

        m_DX12GraphicsCommandList->SetDescriptorHeaps(1, &dx12DescriptorHeap);
    }

    void Context::IASetVertexBuffer(const BufferResource* vertexBuffer)
    {
        if (!vertexBuffer)
        {
            m_DX12GraphicsCommandList->IASetVertexBuffers(0, 1, nullptr);
        }
        else
        {
            const D3D12_VERTEX_BUFFER_VIEW dx12VBV
            {
                .BufferLocation{ static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(vertexBuffer->GetGPUVirtualAddress()) },
                .SizeInBytes{ vertexBuffer->GetConfig().ElementSize * vertexBuffer->GetConfig().ElementCount },
                .StrideInBytes{ vertexBuffer->GetConfig().ElementSize }
            };

            m_DX12GraphicsCommandList->IASetVertexBuffers(0, 1, &dx12VBV);
        }
    }

    void Context::IASetIndexBuffer(const BufferResource* indexBuffer)
    {
        if (!indexBuffer)
        {
            m_DX12GraphicsCommandList->IASetIndexBuffer(nullptr);
        }
        else
        {
            GraphicsFormat format;

            switch (indexBuffer->GetConfig().ElementSize)
            {
                case 2:
                {
                    format = GraphicsFormat::R16UnsignedInt;
                    break;
                }
                case 4:
                {
                    format = GraphicsFormat::R32UnsignedInt;
                    break;
                }
                default:
                {
                    SPIELER_ASSERT(false);
                    break;
                }
            }

            const D3D12_INDEX_BUFFER_VIEW dx12IBV
            {
                .BufferLocation{ static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(indexBuffer->GetGPUVirtualAddress()) },
                .SizeInBytes{ indexBuffer->GetConfig().ElementSize * indexBuffer->GetConfig().ElementCount },
                .Format{ dx12::Convert(format) }
            };

            m_DX12GraphicsCommandList->IASetIndexBuffer(&dx12IBV);
        }
    }

    void Context::IASetPrimitiveTopology(PrimitiveTopology primitiveTopology)
    {
        SPIELER_ASSERT(primitiveTopology != PrimitiveTopology::Unknown);

        m_DX12GraphicsCommandList->IASetPrimitiveTopology(dx12::Convert(primitiveTopology));
    }

    void Context::SetViewport(const Viewport& viewport)
    {
        m_DX12GraphicsCommandList->RSSetViewports(1, reinterpret_cast<const D3D12_VIEWPORT*>(&viewport));
    }

    void Context::SetScissorRect(const ScissorRect& scissorRect)
    {
        const D3D12_RECT d3d12Rect
        {
            .left{ static_cast<LONG>(scissorRect.X) },
            .top{ static_cast<LONG>(scissorRect.Y) },
            .right{ static_cast<LONG>(scissorRect.X + scissorRect.Width) },
            .bottom{ static_cast<LONG>(scissorRect.Y + scissorRect.Height) },
        };

        m_DX12GraphicsCommandList->RSSetScissorRects(1, &d3d12Rect);
    }

    void Context::SetPipelineState(const PipelineState& pso)
    {
        m_DX12GraphicsCommandList->SetPipelineState(pso.GetDX12PipelineState());
    }

    void Context::SetGraphicsRootSignature(const RootSignature& rootSignature)
    {
        m_DX12GraphicsCommandList->SetGraphicsRootSignature(rootSignature.GetDX12RootSignature());
    }

    void Context::SetComputeRootSignature(const RootSignature& rootSignature)
    {
        m_DX12GraphicsCommandList->SetComputeRootSignature(rootSignature.GetDX12RootSignature());
    }

    void Context::SetGraphicsRawConstantBuffer(uint32_t rootParameterIndex, const BufferResource& bufferResource, uint32_t beginElement)
    {
        const D3D12_GPU_VIRTUAL_ADDRESS dx12GPUVirtualAddress{ bufferResource.GetDX12Resource()->GetGPUVirtualAddress() + beginElement * bufferResource.GetConfig().ElementSize };

        m_DX12GraphicsCommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, dx12GPUVirtualAddress);
    }

    void Context::SetGraphicsRawShaderResource(uint32_t rootParameterIndex, const BufferResource& bufferResource, uint32_t beginElement)
    {
        const D3D12_GPU_VIRTUAL_ADDRESS dx12GPUVirtualAddress{ bufferResource.GetDX12Resource()->GetGPUVirtualAddress() + beginElement * bufferResource.GetConfig().ElementSize };

        m_DX12GraphicsCommandList->SetGraphicsRootShaderResourceView(rootParameterIndex, dx12GPUVirtualAddress);
    }

    void Context::SetGraphicsDescriptorTable(uint32_t rootParameterIndex, const TextureShaderResourceView& srv)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE dx12GPUDescriptorHandle{ srv.GetDescriptor().GPU };
        
        m_DX12GraphicsCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, dx12GPUDescriptorHandle);
    }

    void Context::SetRenderTarget(const TextureRenderTargetView& rtv)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{ rtv.GetDescriptor().CPU };

        m_DX12GraphicsCommandList->OMSetRenderTargets(1, &rtvDescriptorHandle, true, nullptr);
    }

    void Context::SetRenderTarget(const TextureRenderTargetView& rtv, const TextureDepthStencilView& dsv)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{ rtv.GetDescriptor().CPU };
        const D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle{ dsv.GetDescriptor().CPU };

        m_DX12GraphicsCommandList->OMSetRenderTargets(1, &rtvDescriptorHandle, true, &dsvDescriptorHandle);
    }

    void Context::ClearRenderTarget(const TextureRenderTargetView& rtv, const DirectX::XMFLOAT4& color)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{ rtv.GetDescriptor().CPU };

        m_DX12GraphicsCommandList->ClearRenderTargetView(rtvDescriptorHandle, reinterpret_cast<const float*>(&color), 0, nullptr);
    }

    void Context::ClearDepthStencil(const TextureDepthStencilView& dsv, float depth, uint8_t stencil)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle{ dsv.GetDescriptor().CPU };

        m_DX12GraphicsCommandList->ClearDepthStencilView(
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
        SPIELER_ASSERT(barrier.Resource->GetDX12Resource());

        const D3D12_RESOURCE_BARRIER d3d12Barrier{ _internal::ConvertToD3D12ResourceBarrier(barrier) };

        m_DX12GraphicsCommandList->ResourceBarrier(1, &d3d12Barrier);
    }

    void Context::SetStencilReferenceValue(uint8_t referenceValue)
    {
        m_DX12GraphicsCommandList->OMSetStencilRef(referenceValue);
    }

    void Context::UploadToBuffer(BufferResource& buffer,const void* data, uint64_t size)
    {
        SPIELER_ASSERT(buffer.GetDX12Resource());
        SPIELER_ASSERT(size);
        SPIELER_ASSERT(data);

        MappedData mappedData{ m_UploadBuffer.Resource, 0 };

        const uint64_t uploadBufferOffset{ m_UploadBuffer.Allocate(size) };

        memcpy_s(mappedData.GetData() + uploadBufferOffset, size, data, size);

        m_DX12GraphicsCommandList->CopyBufferRegion(buffer.GetDX12Resource(), 0, m_UploadBuffer.Resource->GetDX12Resource(), uploadBufferOffset, size);
    }

    void Context::UploadToTexture(TextureResource& texture, std::vector<SubresourceData>& subresources)
    {
        SPIELER_ASSERT(texture.GetDX12Resource());
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
                ComPtr<ID3D12Device> dx12Device;
                texture.GetDX12Resource()->GetDevice(IID_PPV_ARGS(&dx12Device));

                const D3D12_RESOURCE_DESC textureDesc{ texture.GetDX12Resource()->GetDesc() };

                dx12Device->GetCopyableFootprints(
                    &textureDesc,
                    firstSubresource,
                    static_cast<uint32_t>(subresources.size()),
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
        {
            MappedData mappedData{ m_UploadBuffer.Resource, 0 };

            for (size_t i = 0; i < subresources.size(); ++i)
            {
                SPIELER_ASSERT(rowSizes[i] <= static_cast<uint64_t>(-1));

                std::byte* destinationData{ mappedData.GetData() + layouts[i].Offset };
                const std::byte* sourceData{ subresources[i].Data };

                for (uint32_t z = 0; z < layouts[i].Footprint.Depth; ++z)
                {
                    std::byte* destinationSlice{ destinationData + layouts[i].Footprint.RowPitch * static_cast<uint64_t>(rowCounts[i]) * z };
                    const std::byte* sourceSlice{ sourceData + subresources[i].SlicePitch * z };

                    for (uint32_t y = 0; y < rowCounts[i]; ++y)
                    {
                        std::byte* destinationRow{ destinationSlice + static_cast<uint64_t>(layouts[i].Footprint.RowPitch) * y };
                        const std::byte* sourceRow{ sourceSlice + subresources[i].RowPitch * y };

                        memcpy_s(destinationRow, rowSizes[i], sourceRow, rowSizes[i]);
                    }
                }
            }
        }

        // Copy to texture
        for (size_t i = 0; i < subresources.size(); ++i)
        {
            const D3D12_TEXTURE_COPY_LOCATION destination
            {
                .pResource{ texture.GetDX12Resource() },
                .Type{ D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX },
                .SubresourceIndex{ static_cast<uint32_t>(i + firstSubresource) }
            };

            const D3D12_TEXTURE_COPY_LOCATION source
            {
                .pResource{ m_UploadBuffer.Resource->GetDX12Resource() },
                .Type{ D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT },
                .PlacedFootprint{ layouts[i] }
            };

            m_DX12GraphicsCommandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        }
    }

    void Context::DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount /* = 1 */)
    {
        m_DX12GraphicsCommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, 0);
    }

    void Context::CloseCommandList()
    {
        SPIELER_ASSERT(SUCCEEDED(m_DX12GraphicsCommandList->Close()));
    }

    void Context::ExecuteCommandList(bool isNeedToFlushCommandQueue)
    {
        CloseCommandList();

        m_DX12CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(m_DX12GraphicsCommandList.GetAddressOf()));

        if (isNeedToFlushCommandQueue)
        {
            FlushCommandQueue();

            m_UploadBuffer.Offset = 0;
        }
    }

    void Context::FlushCommandQueue()
    {
        m_Fence.Increment();

        SPIELER_ASSERT(SUCCEEDED(m_DX12CommandQueue->Signal(m_Fence.GetDX12Fence(), m_Fence.GetValue())));
        m_Fence.WaitForGPU();
    }

    void Context::ResetCommandList(const PipelineState& pso)
    {
        SPIELER_ASSERT(SUCCEEDED(m_DX12GraphicsCommandList->Reset(m_DX12CommandAllocator.Get(), pso.GetDX12PipelineState())));
    }

    void Context::ResetCommandAllocator()
    {
        SPIELER_ASSERT(SUCCEEDED(m_DX12CommandAllocator->Reset()));
    }

    bool Context::Init(Device& device)
    {
        return InitCommandAllocator(device) && InitCommandList(device) && InitCommandQueue(device);
    }

    bool Context::InitCommandAllocator(Device& device)
    {
        SPIELER_RETURN_IF_FAILED(device.GetDX12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_DX12CommandAllocator)));

        return true;
    }

    bool Context::InitCommandList(Device& device)
    {
        SPIELER_RETURN_IF_FAILED(device.GetDX12Device()->CreateCommandList(
            0, 
            D3D12_COMMAND_LIST_TYPE_DIRECT, 
            m_DX12CommandAllocator.Get(),
            nullptr, 
            IID_PPV_ARGS(&m_DX12GraphicsCommandList)
        ));

        SPIELER_RETURN_IF_FAILED(m_DX12GraphicsCommandList->Close());

        return true;
    }

    bool Context::InitCommandQueue(Device& device)
    {
        const D3D12_COMMAND_QUEUE_DESC desc
        {
            .Type{ D3D12_COMMAND_LIST_TYPE_DIRECT },
            .Priority{ 0 },
            .Flags{ D3D12_COMMAND_QUEUE_FLAG_NONE },
            .NodeMask{ 0 }
        };

        SPIELER_RETURN_IF_FAILED(device.GetDX12Device()->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_DX12CommandQueue)));

        return true;
    }

} // namespace spieler::renderer
