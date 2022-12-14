#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/graphics_command_list.hpp"

#include "spieler/core/common.hpp"

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/mapped_data.hpp"
#include "spieler/graphics/utils.hpp"
#include "spieler/graphics/root_signature.hpp"
#include "spieler/graphics/resource_barrier.hpp"
#include "spieler/graphics/pipeline_state.hpp"

#include "platform/dx12/dx12_common.hpp"

namespace spieler
{

    namespace _internal
    {

        static D3D12_RESOURCE_BARRIER ConvertToDX12ResourceBarrier(const TransitionResourceBarrier& barrier)
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

	GraphicsCommandList::GraphicsCommandList(Device& device)
	{
		const D3D12_COMMAND_LIST_TYPE commandListType{ D3D12_COMMAND_LIST_TYPE_DIRECT };

		SPIELER_ASSERT(SUCCEEDED(device.GetDX12Device()->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&m_DX12CommandAllocator))));

		SPIELER_ASSERT(SUCCEEDED(device.GetDX12Device()->CreateCommandList(
			0,
			commandListType,
			m_DX12CommandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&m_DX12GraphicsCommandList)
		)));

		Close();

        // Upload Buffer
        {
            const BufferResource::Config uploadBufferConfig
            {
                .ElementSize{ sizeof(std::byte) },
                .ElementCount{ static_cast<uint32_t>(ConvertMBToBytes(120)) },
                .Flags{ BufferResource::Flags::Dynamic }
            };

            m_UploadBuffer = BufferResource::Create(device, uploadBufferConfig);
        }
	}

	void GraphicsCommandList::Close()
	{
		SPIELER_ASSERT(SUCCEEDED(m_DX12GraphicsCommandList->Close()));
	}

	void GraphicsCommandList::Reset(const PipelineState* const pso)
	{
        // Reset CommandAllocator
        SPIELER_ASSERT(SUCCEEDED(m_DX12CommandAllocator->Reset()));
        
        // Reset GraphicsCommandList
		ID3D12PipelineState* dx12PSO{ pso ? pso->GetDX12PipelineState() : nullptr };
		SPIELER_ASSERT(SUCCEEDED(m_DX12GraphicsCommandList->Reset(m_DX12CommandAllocator.Get(), dx12PSO)));
	}

    uint64_t GraphicsCommandList::AllocateInUploadBuffer(uint64_t size, uint64_t alignment)
    {
        const uint64_t offset{ alignment == 0 ? m_UploadBufferOffset : utils::Align(m_UploadBufferOffset, alignment) };

        SPIELER_ASSERT(offset + size <= m_UploadBuffer->GetConfig().ElementCount);
        m_UploadBufferOffset = offset + size;

        return offset;
    }

    void GraphicsCommandList::SetDescriptorHeap(const DescriptorHeap& descriptorHeap)
    {
        ID3D12DescriptorHeap* dx12DescriptorHeap{ descriptorHeap.GetDX12DescriptorHeap() };

        m_DX12GraphicsCommandList->SetDescriptorHeaps(1, &dx12DescriptorHeap);
    }

    void GraphicsCommandList::IASetVertexBuffer(const BufferResource* vertexBuffer)
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

    void GraphicsCommandList::IASetIndexBuffer(const BufferResource* indexBuffer)
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

    void GraphicsCommandList::IASetPrimitiveTopology(PrimitiveTopology primitiveTopology)
    {
        SPIELER_ASSERT(primitiveTopology != PrimitiveTopology::Unknown);

        m_DX12GraphicsCommandList->IASetPrimitiveTopology(dx12::Convert(primitiveTopology));
    }

    void GraphicsCommandList::SetViewport(const Viewport& viewport)
    {
        m_DX12GraphicsCommandList->RSSetViewports(1, reinterpret_cast<const D3D12_VIEWPORT*>(&viewport));
    }

    void GraphicsCommandList::SetScissorRect(const ScissorRect& scissorRect)
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

    void GraphicsCommandList::SetPipelineState(const PipelineState& pso)
    {
        m_DX12GraphicsCommandList->SetPipelineState(pso.GetDX12PipelineState());
    }

    void GraphicsCommandList::SetGraphicsRootSignature(const RootSignature& rootSignature)
    {
        m_DX12GraphicsCommandList->SetGraphicsRootSignature(rootSignature.GetDX12RootSignature());
    }

    void GraphicsCommandList::SetComputeRootSignature(const RootSignature& rootSignature)
    {
        m_DX12GraphicsCommandList->SetComputeRootSignature(rootSignature.GetDX12RootSignature());
    }

    void GraphicsCommandList::SetCompute32BitConstants(uint32_t rootParameterIndex, const void* data, uint64_t count, uint64_t offsetCount)
    {
        m_DX12GraphicsCommandList->SetComputeRoot32BitConstants(rootParameterIndex, static_cast<UINT>(count), data, static_cast<UINT>(offsetCount));
    }

    void GraphicsCommandList::SetGraphicsRawConstantBuffer(uint32_t rootParameterIndex, const BufferResource& bufferResource, uint32_t beginElement)
    {
        const D3D12_GPU_VIRTUAL_ADDRESS dx12GPUVirtualAddress{ bufferResource.GetDX12Resource()->GetGPUVirtualAddress() + beginElement * bufferResource.GetConfig().ElementSize };

        m_DX12GraphicsCommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, dx12GPUVirtualAddress);
    }

    void GraphicsCommandList::SetGraphicsRawShaderResource(uint32_t rootParameterIndex, const BufferResource& bufferResource, uint32_t beginElement)
    {
        const D3D12_GPU_VIRTUAL_ADDRESS dx12GPUVirtualAddress{ bufferResource.GetDX12Resource()->GetGPUVirtualAddress() + beginElement * bufferResource.GetConfig().ElementSize };

        m_DX12GraphicsCommandList->SetGraphicsRootShaderResourceView(rootParameterIndex, dx12GPUVirtualAddress);
    }

    void GraphicsCommandList::SetGraphicsDescriptorTable(uint32_t rootParameterIndex, const TextureShaderResourceView& firstSRV)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE dx12GPUDescriptorHandle{ firstSRV.GetDescriptor().GPU };

        m_DX12GraphicsCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, dx12GPUDescriptorHandle);
    }

    void GraphicsCommandList::SetComputeDescriptorTable(uint32_t rootParameterIndex, const TextureShaderResourceView& firstSRV)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE dx12GPUDescriptorHandle{ firstSRV.GetDescriptor().GPU };

        m_DX12GraphicsCommandList->SetComputeRootDescriptorTable(rootParameterIndex, dx12GPUDescriptorHandle);
    }

    void GraphicsCommandList::SetComputeDescriptorTable(uint32_t rootParameterIndex, const TextureUnorderedAccessView& firstUAV)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE dx12GPUDescriptorHandle{ firstUAV.GetDescriptor().GPU };

        m_DX12GraphicsCommandList->SetComputeRootDescriptorTable(rootParameterIndex, dx12GPUDescriptorHandle);
    }

    void GraphicsCommandList::SetRenderTarget(const TextureRenderTargetView& rtv)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{ rtv.GetDescriptor().CPU };

        m_DX12GraphicsCommandList->OMSetRenderTargets(1, &rtvDescriptorHandle, true, nullptr);
    }

    void GraphicsCommandList::SetRenderTarget(const TextureRenderTargetView& rtv, const TextureDepthStencilView& dsv)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{ rtv.GetDescriptor().CPU };
        const D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle{ dsv.GetDescriptor().CPU };

        m_DX12GraphicsCommandList->OMSetRenderTargets(1, &rtvDescriptorHandle, true, &dsvDescriptorHandle);
    }

    void GraphicsCommandList::ClearRenderTarget(const TextureRenderTargetView& rtv, const DirectX::XMFLOAT4& color)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{ rtv.GetDescriptor().CPU };

        m_DX12GraphicsCommandList->ClearRenderTargetView(rtvDescriptorHandle, reinterpret_cast<const float*>(&color), 0, nullptr);
    }

    void GraphicsCommandList::ClearDepthStencil(const TextureDepthStencilView& dsv, float depth, uint8_t stencil)
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

    void GraphicsCommandList::SetResourceBarrier(const TransitionResourceBarrier& barrier)
    {
        SPIELER_ASSERT(barrier.Resource);
        SPIELER_ASSERT(barrier.Resource->GetDX12Resource());

        const D3D12_RESOURCE_BARRIER dx12Barrier{ _internal::ConvertToDX12ResourceBarrier(barrier) };

        m_DX12GraphicsCommandList->ResourceBarrier(1, &dx12Barrier);
    }

    void GraphicsCommandList::SetStencilReferenceValue(uint8_t referenceValue)
    {
        m_DX12GraphicsCommandList->OMSetStencilRef(referenceValue);
    }

    void GraphicsCommandList::UploadToBuffer(BufferResource& buffer, const void* data, uint64_t size)
    {
        SPIELER_ASSERT(buffer.GetDX12Resource());
        SPIELER_ASSERT(size);
        SPIELER_ASSERT(data);

        MappedData mappedData{ m_UploadBuffer, 0 };

        const uint64_t uploadBufferOffset{ AllocateInUploadBuffer(size) };

        memcpy_s(mappedData.GetData() + uploadBufferOffset, size, data, size);

        m_DX12GraphicsCommandList->CopyBufferRegion(buffer.GetDX12Resource(), 0, m_UploadBuffer->GetDX12Resource(), uploadBufferOffset, size);
    }

    void GraphicsCommandList::UploadToTexture(TextureResource& texture, std::vector<SubresourceData>& subresources)
    {
        SPIELER_ASSERT(texture.GetDX12Resource());
        SPIELER_ASSERT(!subresources.empty());

        const uint32_t firstSubresource = 0;

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

                const D3D12_RESOURCE_DESC textureDesc = texture.GetDX12Resource()->GetDesc();
                const uint64_t offset = AllocateInUploadBuffer(0, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

                dx12Device->GetCopyableFootprints(
                    &textureDesc,
                    firstSubresource,
                    static_cast<uint32_t>(subresources.size()),
                    offset,
                    layouts.data(),
                    rowCounts.data(),
                    rowSizes.data(),
                    &requiredUploadBufferSize
                );
            }

            AllocateInUploadBuffer(requiredUploadBufferSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        }

        // Copying subresources to UploadBuffer
        // Go down to rows and copy it
        {
            MappedData mappedData{ m_UploadBuffer, 0 };

            for (size_t i = 0; i < subresources.size(); ++i)
            {
                const SubresourceData& subresource = subresources[i];
                const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout = layouts[i];

                // SubResource data
                std::byte* destinationData = mappedData.GetData() + layout.Offset;
                const std::byte* sourceData = subresource.Data;

                for (uint32_t sliceIndex = 0; sliceIndex < layout.Footprint.Depth; ++sliceIndex)
                {
                    const uint64_t rowCount = rowCounts[i];
                    const uint64_t destinationSlicePitch = layout.Footprint.RowPitch * rowCount;

                    // Slice data
                    std::byte* destinationSliceData = destinationData + destinationSlicePitch * sliceIndex;
                    const std::byte* sourceSliceData = sourceData + subresource.SlicePitch * sliceIndex;

                    for (uint32_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                    {
                        const uint64_t destinationRowPitch = layout.Footprint.RowPitch;

                        // Row data
                        std::byte* destinationRowData = destinationSliceData + destinationRowPitch * rowIndex;
                        const std::byte* sourceRowData = sourceSliceData + subresource.RowPitch * rowIndex;
                        
                        const uint64_t rowSize = rowSizes[i];
                        memcpy_s(destinationRowData, rowSize, sourceRowData, rowSize);
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
                .pResource{ m_UploadBuffer->GetDX12Resource() },
                .Type{ D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT },
                .PlacedFootprint{ layouts[i] }
            };

            m_DX12GraphicsCommandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        }
    }

    void GraphicsCommandList::CopyResource(Resource& to, Resource& from)
    {
        m_DX12GraphicsCommandList->CopyResource(to.GetDX12Resource(), from.GetDX12Resource());
    }

    void GraphicsCommandList::DrawVertexed(uint32_t vertexCount, uint32_t startVertexLocation, uint32_t instanceCount)
    {
        m_DX12GraphicsCommandList->DrawInstanced(vertexCount, instanceCount, startVertexLocation, 0);
    }

    void GraphicsCommandList::DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount)
    {
        m_DX12GraphicsCommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, 0);
    }

    void GraphicsCommandList::Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ)
    {
        m_DX12GraphicsCommandList->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
    }

} // namespace spieler
