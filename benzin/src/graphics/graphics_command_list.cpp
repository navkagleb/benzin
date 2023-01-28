#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/graphics_command_list.hpp"

#include "benzin/core/common.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/mapped_data.hpp"
#include "benzin/graphics/utils.hpp"
#include "benzin/graphics/root_signature.hpp"
#include "benzin/graphics/pipeline_state.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    static uint32_t g_GraphicsCommandListCounter = 0;

	GraphicsCommandList::GraphicsCommandList(Device& device, const std::string& debugName)
	{
		const D3D12_COMMAND_LIST_TYPE commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT;

        BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&m_D3D12CommandAllocator)));

        BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateCommandList(
			0,
			commandListType,
			m_D3D12CommandAllocator,
			nullptr,
			IID_PPV_ARGS(&m_D3D12GraphicsCommandList)
		));

        SetDebugName(debugName.empty() ? std::to_string(g_GraphicsCommandListCounter) : debugName);

		Close();

        // Upload Buffer
        {
            const BufferResource::Config uploadBufferConfig
            {
                .ElementSize{ sizeof(std::byte) },
                .ElementCount{ static_cast<uint32_t>(ConvertMBToBytes(120)) },
                .Flags{ BufferResource::Flags::Dynamic }
            };

            m_UploadBuffer = device.CreateBufferResource(uploadBufferConfig, "UploadBuffer_" + GetDebugName());
        }
	}

    GraphicsCommandList::~GraphicsCommandList()
    {
        BENZIN_INFO("{} destroyed", GetDebugName());

        SafeReleaseD3D12Object(m_D3D12GraphicsCommandList);
        SafeReleaseD3D12Object(m_D3D12CommandAllocator);
    }

	void GraphicsCommandList::Close()
	{
        BENZIN_D3D12_ASSERT(m_D3D12GraphicsCommandList->Close());
	}

	void GraphicsCommandList::Reset(const PipelineState* const pso)
	{
        // Reset CommandAllocator
        BENZIN_D3D12_ASSERT(m_D3D12CommandAllocator->Reset());
        
        // Reset GraphicsCommandList
		ID3D12PipelineState* d3d12PSO = pso ? pso->GetD3D12PipelineState() : nullptr;
		BENZIN_D3D12_ASSERT(m_D3D12GraphicsCommandList->Reset(m_D3D12CommandAllocator, d3d12PSO));
	}

    void GraphicsCommandList::SetDescriptorHeaps(const DescriptorManager& descriptorManager)
    {
        ID3D12DescriptorHeap* const d3d12DescriptorHeaps[]
        { 
            descriptorManager.GetD3D12ResourceDescriptorHeap(),
            descriptorManager.GetD3D12SamplerDescriptorHeap()
        };

        m_D3D12GraphicsCommandList->SetDescriptorHeaps(static_cast<UINT>(std::size(d3d12DescriptorHeaps)), d3d12DescriptorHeaps);
    }

    void GraphicsCommandList::IASetVertexBuffer(const BufferResource* vertexBuffer)
    {
        if (!vertexBuffer)
        {
            m_D3D12GraphicsCommandList->IASetVertexBuffers(0, 1, nullptr);
        }
        else
        {
            const D3D12_VERTEX_BUFFER_VIEW d3d12VBV
            {
                .BufferLocation{ static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(vertexBuffer->GetGPUVirtualAddress()) },
                .SizeInBytes{ vertexBuffer->GetConfig().ElementSize * vertexBuffer->GetConfig().ElementCount },
                .StrideInBytes{ vertexBuffer->GetConfig().ElementSize }
            };

            m_D3D12GraphicsCommandList->IASetVertexBuffers(0, 1, &d3d12VBV);
        }
    }

    void GraphicsCommandList::IASetIndexBuffer(const BufferResource* indexBuffer)
    {
        if (!indexBuffer)
        {
            m_D3D12GraphicsCommandList->IASetIndexBuffer(nullptr);
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
                    BENZIN_ASSERT(false);
                    break;
                }
            }

            const D3D12_INDEX_BUFFER_VIEW d3d12IBV
            {
                .BufferLocation{ static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(indexBuffer->GetGPUVirtualAddress()) },
                .SizeInBytes{ indexBuffer->GetConfig().ElementSize * indexBuffer->GetConfig().ElementCount },
                .Format{ static_cast<DXGI_FORMAT>(format) }
            };

            m_D3D12GraphicsCommandList->IASetIndexBuffer(&d3d12IBV);
        }
    }

    void GraphicsCommandList::IASetPrimitiveTopology(PrimitiveTopology primitiveTopology)
    {
        BENZIN_ASSERT(primitiveTopology != PrimitiveTopology::Unknown);

        m_D3D12GraphicsCommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(primitiveTopology));
    }

    void GraphicsCommandList::SetViewport(const Viewport& viewport)
    {
        m_D3D12GraphicsCommandList->RSSetViewports(1, reinterpret_cast<const D3D12_VIEWPORT*>(&viewport));
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

        m_D3D12GraphicsCommandList->RSSetScissorRects(1, &d3d12Rect);
    }

    void GraphicsCommandList::SetPipelineState(const PipelineState& pso)
    {
        m_D3D12GraphicsCommandList->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void GraphicsCommandList::SetGraphicsRootSignature(const RootSignature& rootSignature)
    {
        m_D3D12GraphicsCommandList->SetGraphicsRootSignature(rootSignature.GetD3D12RootSignature());
    }

    void GraphicsCommandList::SetComputeRootSignature(const RootSignature& rootSignature)
    {
        m_D3D12GraphicsCommandList->SetComputeRootSignature(rootSignature.GetD3D12RootSignature());
    }

    void GraphicsCommandList::SetCompute32BitConstants(uint32_t rootParameterIndex, const void* data, uint64_t count, uint64_t offsetCount)
    {
        m_D3D12GraphicsCommandList->SetComputeRoot32BitConstants(rootParameterIndex, static_cast<UINT>(count), data, static_cast<UINT>(offsetCount));
    }

    void GraphicsCommandList::SetGraphicsRawConstantBuffer(uint32_t rootParameterIndex, const BufferResource& bufferResource, uint32_t beginElement)
    {
        const D3D12_GPU_VIRTUAL_ADDRESS d3d12GPUVirtualAddress{ bufferResource.GetD3D12Resource()->GetGPUVirtualAddress() + beginElement * bufferResource.GetConfig().ElementSize };

        m_D3D12GraphicsCommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, d3d12GPUVirtualAddress);
    }

    void GraphicsCommandList::SetGraphicsRawShaderResource(uint32_t rootParameterIndex, const BufferResource& bufferResource, uint32_t beginElement)
    {
        const D3D12_GPU_VIRTUAL_ADDRESS d3d12GPUVirtualAddress{ bufferResource.GetD3D12Resource()->GetGPUVirtualAddress() + beginElement * bufferResource.GetConfig().ElementSize };

        m_D3D12GraphicsCommandList->SetGraphicsRootShaderResourceView(rootParameterIndex, d3d12GPUVirtualAddress);
    }

    void GraphicsCommandList::SetGraphicsDescriptorTable(uint32_t rootParameterIndex, const Descriptor& firstDescriptor)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE d3d12GPUDescriptorHandle{ firstDescriptor.GetGPUHandle() };

        m_D3D12GraphicsCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, d3d12GPUDescriptorHandle);
    }

    void GraphicsCommandList::SetComputeDescriptorTable(uint32_t rootParameterIndex, const Descriptor& firstDescriptor)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE d3d12GPUDescriptorHandle{ firstDescriptor.GetGPUHandle() };

        m_D3D12GraphicsCommandList->SetComputeRootDescriptorTable(rootParameterIndex, d3d12GPUDescriptorHandle);
    }

    void GraphicsCommandList::SetRenderTarget(const Descriptor& rtv)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12RTVDescriptorHandle{ rtv.GetCPUHandle()};

        m_D3D12GraphicsCommandList->OMSetRenderTargets(1, &d3d12RTVDescriptorHandle, true, nullptr);
    }

    void GraphicsCommandList::SetRenderTarget(const Descriptor& rtv, const Descriptor& dsv)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12RTVDescriptorHandle{ rtv.GetCPUHandle() };
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12DSVDescriptorHandle{ dsv.GetCPUHandle() };

        m_D3D12GraphicsCommandList->OMSetRenderTargets(1, &d3d12RTVDescriptorHandle, true, &d3d12DSVDescriptorHandle);
    }

    void GraphicsCommandList::ClearRenderTarget(const Descriptor& rtv, const DirectX::XMFLOAT4& color)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12RTVDescriptorHandle{ rtv.GetCPUHandle() };

        m_D3D12GraphicsCommandList->ClearRenderTargetView(d3d12RTVDescriptorHandle, reinterpret_cast<const float*>(&color), 0, nullptr);
    }

    void GraphicsCommandList::ClearDepthStencil(const Descriptor& dsv, float depth, uint8_t stencil)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12DSVDescriptorHandle{ dsv.GetCPUHandle() };

        m_D3D12GraphicsCommandList->ClearDepthStencilView(
            d3d12DSVDescriptorHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            depth,
            stencil,
            0,
            nullptr
        );
    }

    void GraphicsCommandList::SetResourceBarrier(Resource& resource, Resource::State resourceStateAfter)
    {
        BENZIN_ASSERT(resource.GetD3D12Resource());

        const D3D12_RESOURCE_BARRIER d3d12ResourceBarrier
        {
            .Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
            .Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
            .Transition
            {
                .pResource{ resource.GetD3D12Resource() },
                .Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
                .StateBefore{ static_cast<D3D12_RESOURCE_STATES>(resource.GetCurrentState()) },
                .StateAfter{ static_cast<D3D12_RESOURCE_STATES>(resourceStateAfter) }
            }
        };

        m_D3D12GraphicsCommandList->ResourceBarrier(1, &d3d12ResourceBarrier);

        resource.SetCurrentState(resourceStateAfter);
    }

    void GraphicsCommandList::SetStencilReferenceValue(uint8_t referenceValue)
    {
        m_D3D12GraphicsCommandList->OMSetStencilRef(referenceValue);
    }

    void GraphicsCommandList::UploadToBuffer(BufferResource& buffer, const void* data, uint64_t size)
    {
        BENZIN_ASSERT(buffer.GetD3D12Resource());
        BENZIN_ASSERT(size);
        BENZIN_ASSERT(data);

        MappedData mappedData{ *m_UploadBuffer };

        const uint64_t uploadBufferOffset{ AllocateInUploadBuffer(size) };

        memcpy_s(mappedData.GetData() + uploadBufferOffset, size, data, size);

        m_D3D12GraphicsCommandList->CopyBufferRegion(buffer.GetD3D12Resource(), 0, m_UploadBuffer->GetD3D12Resource(), uploadBufferOffset, size);
    }

    void GraphicsCommandList::UploadToTexture(TextureResource& texture, const std::vector<SubResourceData>& subResources)
    {
        struct CopyableFootprints
        {
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> D3D12Layouts; // RowPitch aligned by D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
            std::vector<uint32_t> RowCounts;
            std::vector<uint64_t> RowSizes;

            CopyableFootprints(size_t size)
            {
                D3D12Layouts.resize(size);
                RowCounts.resize(size);
                RowSizes.resize(size);
            }
        };

        BENZIN_ASSERT(texture.GetD3D12Resource());
        BENZIN_ASSERT(!subResources.empty());

        const uint32_t firstSubresource = 0;

        CopyableFootprints copyableFootprits{ subResources.size() };

        // Init CopyableFootprints and allocate memory in UploadBuffer
        {
            uint64_t resourceSize = 0;

            {
                ComPtr<ID3D12Device> d3d12Device;
                texture.GetD3D12Resource()->GetDevice(IID_PPV_ARGS(&d3d12Device));

                const D3D12_RESOURCE_DESC d3d12TextureDesc = texture.GetD3D12Resource()->GetDesc();
                const uint64_t offset = AllocateInUploadBuffer(0, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

                d3d12Device->GetCopyableFootprints(
                    &d3d12TextureDesc,
                    firstSubresource,
                    static_cast<uint32_t>(subResources.size()),
                    offset,
                    copyableFootprits.D3D12Layouts.data(),
                    copyableFootprits.RowCounts.data(),
                    copyableFootprits.RowSizes.data(),
                    &resourceSize
                );
            }

            AllocateInUploadBuffer(resourceSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        }

        // Copying subresources to UploadBuffer
        // Go down to rows and copy it
        {
            MappedData mappedData{ *m_UploadBuffer };

            for (size_t subResourceIndex = 0; subResourceIndex < subResources.size(); ++subResourceIndex)
            {
                const SubResourceData& subResource = subResources[subResourceIndex];
                const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& d3d12Layout = copyableFootprits.D3D12Layouts[subResourceIndex];

                // SubResource data
                std::byte* destinationData = mappedData.GetData() + d3d12Layout.Offset;
                const std::byte* sourceData = subResource.Data;

                for (uint32_t sliceIndex = 0; sliceIndex < d3d12Layout.Footprint.Depth; ++sliceIndex)
                {
                    const uint64_t rowCount = copyableFootprits.RowCounts[subResourceIndex];
                    const uint64_t destinationSlicePitch = d3d12Layout.Footprint.RowPitch * rowCount;

                    // Slice data
                    std::byte* destinationSliceData = destinationData + destinationSlicePitch * sliceIndex;
                    const std::byte* sourceSliceData = sourceData + subResource.SlicePitch * sliceIndex;

                    for (uint32_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                    {
                        const uint64_t destinationRowPitch = d3d12Layout.Footprint.RowPitch;

                        // Row data
                        std::byte* destinationRowData = destinationSliceData + destinationRowPitch * rowIndex;
                        const std::byte* sourceRowData = sourceSliceData + subResource.RowPitch * rowIndex;
                        
                        const uint64_t rowSize = copyableFootprits.RowSizes[subResourceIndex];
                        memcpy_s(destinationRowData, rowSize, sourceRowData, rowSize);
                    }
                }
            }
        }

        // Copy to texture
        for (size_t i = 0; i < subResources.size(); ++i)
        {
            const D3D12_TEXTURE_COPY_LOCATION destination
            {
                .pResource{ texture.GetD3D12Resource() },
                .Type{ D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX },
                .SubresourceIndex{ static_cast<uint32_t>(i + firstSubresource) }
            };

            const D3D12_TEXTURE_COPY_LOCATION source
            {
                .pResource{ m_UploadBuffer->GetD3D12Resource() },
                .Type{ D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT },
                .PlacedFootprint{ copyableFootprits.D3D12Layouts[i] }
            };

            m_D3D12GraphicsCommandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        }
    }

    void GraphicsCommandList::CopyResource(Resource& to, Resource& from)
    {
        m_D3D12GraphicsCommandList->CopyResource(to.GetD3D12Resource(), from.GetD3D12Resource());
    }

    void GraphicsCommandList::DrawVertexed(uint32_t vertexCount, uint32_t startVertexLocation, uint32_t instanceCount)
    {
        m_D3D12GraphicsCommandList->DrawInstanced(vertexCount, instanceCount, startVertexLocation, 0);
    }

    void GraphicsCommandList::DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount)
    {
        m_D3D12GraphicsCommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, 0);
    }

    void GraphicsCommandList::Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ)
    {
        m_D3D12GraphicsCommandList->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
    }

    uint64_t GraphicsCommandList::AllocateInUploadBuffer(uint64_t size, uint64_t alignment)
    {
        const uint64_t offset = alignment == 0 ? m_UploadBufferOffset : utils::Align(m_UploadBufferOffset, alignment);

        BENZIN_ASSERT(offset + size <= m_UploadBuffer->GetConfig().ElementCount);
        m_UploadBufferOffset = offset + size;

        return offset;
    }

} // namespace benzin
