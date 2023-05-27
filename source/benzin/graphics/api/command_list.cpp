#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/command_list.hpp"

#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/api/pipeline_state.hpp"
#include "benzin/graphics/api/buffer.hpp"
#include "benzin/graphics/api/texture.hpp"
#include "benzin/graphics/api/descriptor_manager.hpp"
#include "benzin/graphics/api/utils.hpp"
#include "benzin/graphics/mapped_data.hpp"

#include "benzin/core/common.hpp"

namespace benzin
{

    // CommandList
    CommandList::CommandList(Device& device, D3D12_COMMAND_LIST_TYPE d3d12CommandListType)
    {
        BENZIN_ASSERT(device.GetD3D12Device());

        BENZIN_HR_ASSERT(device.GetD3D12Device()->CreateCommandList1(
            0,
            d3d12CommandListType,
            D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&m_D3D12GraphicsCommandList)
        ));
    }

    CommandList::~CommandList()
    {
        dx::SafeRelease(m_D3D12GraphicsCommandList);
    }

    void CommandList::SetResourceBarrier(Resource& resource, Resource::State resourceStateAfter)
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

    void CommandList::CopyResource(Resource& to, Resource& from)
    {
        m_D3D12GraphicsCommandList->CopyResource(to.GetD3D12Resource(), from.GetD3D12Resource());
    }

    // CopyCommandList
    CopyCommandList::CopyCommandList(Device& device)
        : CommandList{ device, D3D12_COMMAND_LIST_TYPE_COPY }
    {}

    void CopyCommandList::UpdateBuffer(BufferResource& buffer, std::span<const std::byte> data, size_t offsetInBytes)
    {
        BENZIN_ASSERT(buffer.GetD3D12Resource());
        BENZIN_ASSERT(!data.empty());

        BENZIN_ASSERT(m_UploadBuffer->GetD3D12Resource());

        const size_t uploadBufferOffset = AllocateInUploadBuffer(data.size_bytes());

        //{
        MappedData mappedData{ *m_UploadBuffer };
        mappedData.Write(data, uploadBufferOffset);
        //}

        m_D3D12GraphicsCommandList->CopyBufferRegion(
            buffer.GetD3D12Resource(),
            offsetInBytes,
            m_UploadBuffer->GetD3D12Resource(),
            uploadBufferOffset,
            data.size_bytes()
        );
    }

    void CopyCommandList::UpdateTexture(TextureResource& texture, const std::vector<SubResourceData>& subResources)
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

        constexpr uint32_t firstSubresource = 0;

        CopyableFootprints copyableFootprits{ subResources.size() };

        // Init CopyableFootprints and allocate memory in UploadBuffer
        {
            uint64_t resourceSize = 0;

            {
                ComPtr<ID3D12Device> d3d12Device;
                texture.GetD3D12Resource()->GetDevice(IID_PPV_ARGS(&d3d12Device));

                const D3D12_RESOURCE_DESC d3d12TextureDesc = texture.GetD3D12Resource()->GetDesc();
                const size_t offset = AllocateInUploadBuffer(0, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

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
            MappedData uploadBuffer{ *m_UploadBuffer };

            for (size_t subResourceIndex = 0; subResourceIndex < subResources.size(); ++subResourceIndex)
            {
                const SubResourceData& subResource = subResources[subResourceIndex];
                const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& d3d12Layout = copyableFootprits.D3D12Layouts[subResourceIndex];

                // SubResource data
                const size_t destinationOffset = d3d12Layout.Offset;
                const std::byte* sourceData = subResource.Data;

                for (uint32_t sliceIndex = 0; sliceIndex < d3d12Layout.Footprint.Depth; ++sliceIndex)
                {
                    const uint64_t rowCount = copyableFootprits.RowCounts[subResourceIndex];
                    const uint64_t destinationSlicePitch = d3d12Layout.Footprint.RowPitch * rowCount;

                    // Slice data
                    const size_t destinationSliceOffset = destinationOffset + destinationSlicePitch * sliceIndex;
                    const std::byte* sourceSliceData = sourceData + subResource.SlicePitch * sliceIndex;

                    for (uint32_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                    {
                        const size_t destinationRowPitch = d3d12Layout.Footprint.RowPitch;

                        // Row data
                        const size_t destinationRowOffset = destinationSliceOffset + destinationRowPitch * rowIndex;
                        const std::byte* sourceRowData = sourceSliceData + subResource.RowPitch * rowIndex;

                        const size_t rowSizeInBytes = copyableFootprits.RowSizes[subResourceIndex];
                        uploadBuffer.Write(std::span{ sourceRowData, rowSizeInBytes }, destinationRowOffset);
                    }
                }
            }
        }

        //SetResourceBarrier(texture, Resource::State::CopyDestination);

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

        //SetResourceBarrier(texture, Resource::State::Present);
    }

    void CopyCommandList::UpdateTextureTopMip(TextureResource& texture, const std::byte* data)
    {
        const TextureResource::Config& config = texture.GetConfig();

        const uint32_t pixelSizeInBytes = GetFormatSizeInBytes(config.Format);
        const SubResourceData topMipSubResource
        {
            .Data{ data },
            .RowPitch{ pixelSizeInBytes * config.Width },
            .SlicePitch{ pixelSizeInBytes * config.Width * config.Height }
        };

        UpdateTexture(texture, { topMipSubResource });
    }

    void CopyCommandList::CreateUploadBuffer(Device& device, uint32_t size)
    {
        const BufferResource::Config config
        {
            .ElementSize{ sizeof(std::byte) },
            .ElementCount{ size },
            .Flags{ BufferResource::Flags::Dynamic }
        };

        m_UploadBuffer = std::make_unique<BufferResource>(device, config);
    }

    void CopyCommandList::ReleaseUploadBuffer()
    {
        m_UploadBuffer.release();
        m_UploadBufferOffset = 0;
    }

    uint64_t CopyCommandList::AllocateInUploadBuffer(size_t size, size_t alignment)
    {
        const size_t offset = alignment == 0 ? m_UploadBufferOffset : Align(m_UploadBufferOffset, alignment);

        BENZIN_ASSERT(offset + size <= m_UploadBuffer->GetConfig().ElementCount);
        m_UploadBufferOffset = offset + size;

        return offset;
    }

    // ComputeCommandList
    ComputeCommandList::ComputeCommandList(Device& device)
        : CommandList{ device, D3D12_COMMAND_LIST_TYPE_COMPUTE }
    {}

    void ComputeCommandList::SetRootConstant(uint32_t rootIndex, uint32_t value)
    {
        const uint32_t rootParameterIndex = 0;

        m_D3D12GraphicsCommandList->SetComputeRoot32BitConstant(rootParameterIndex, value, rootIndex);
    }

    void ComputeCommandList::SetRootConstantBuffer(uint32_t rootIndex, const Descriptor& cbv)
    {
        BENZIN_ASSERT(cbv.IsValid());

        SetRootConstant(rootIndex, cbv.GetHeapIndex());
    }

    void ComputeCommandList::SetRootShaderResource(uint32_t rootIndex, const Descriptor& srv)
    {
        BENZIN_ASSERT(srv.IsValid());

        SetRootConstant(rootIndex, srv.GetHeapIndex());
    }

    void ComputeCommandList::SetRootUnorderedAccess(uint32_t rootIndex, const Descriptor& uav)
    {
        BENZIN_ASSERT(uav.IsValid());

        SetRootConstant(rootIndex, uav.GetHeapIndex());
    }

    void ComputeCommandList::SetPipelineState(const PipelineState& pso)
    {
        BENZIN_ASSERT(pso.GetD3D12PipelineState());

        m_D3D12GraphicsCommandList->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void ComputeCommandList::Dispatch(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadPerGroupCount)
    {
        BENZIN_ASSERT(threadPerGroupCount.x != 0);
        BENZIN_ASSERT(threadPerGroupCount.y != 0);
        BENZIN_ASSERT(threadPerGroupCount.z != 0);

        const uint32_t groupCountX = AlignThreadGroupCount(dimension.x, threadPerGroupCount.x);
        const uint32_t groupCountY = AlignThreadGroupCount(dimension.y, threadPerGroupCount.y);
        const uint32_t groupCountZ = AlignThreadGroupCount(dimension.z, threadPerGroupCount.z);

        m_D3D12GraphicsCommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
    }

    // GraphicsCommandList
	GraphicsCommandList::GraphicsCommandList(Device& device)
        : CommandList{ device, D3D12_COMMAND_LIST_TYPE_DIRECT }
    {}

    void GraphicsCommandList::SetRootConstant(uint32_t rootIndex, uint32_t value)
    {
        const uint32_t rootParameterIndex = 0;

        m_D3D12GraphicsCommandList->SetGraphicsRoot32BitConstant(rootParameterIndex, value, rootIndex);
    }

    void GraphicsCommandList::SetRootConstantBuffer(uint32_t rootIndex, const Descriptor& cbv)
    {
        BENZIN_ASSERT(cbv.IsValid());

        SetRootConstant(rootIndex, cbv.GetHeapIndex());
    }

    void GraphicsCommandList::SetRootShaderResource(uint32_t rootIndex, const Descriptor& srv)
    {
        BENZIN_ASSERT(srv.IsValid());

        SetRootConstant(rootIndex, srv.GetHeapIndex());
    }

    void GraphicsCommandList::SetPipelineState(const PipelineState& pso)
    {
        BENZIN_ASSERT(pso.GetD3D12PipelineState());

        m_D3D12GraphicsCommandList->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void GraphicsCommandList::SetPrimitiveTopology(PrimitiveTopology primitiveTopology)
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

    void GraphicsCommandList::SetRenderTargets(const std::vector<Descriptor>& rtvs, const Descriptor* dsv)
    {
        constexpr bool isRenderTargetContiguous = false;

        BENZIN_ASSERT(rtvs.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> d3d12RTVDescriptorHandles;
        d3d12RTVDescriptorHandles.reserve(rtvs.size());

        for (const Descriptor& descriptor : rtvs)
        {
            d3d12RTVDescriptorHandles.emplace_back(descriptor.m_CPUHandle);
        }

        if (dsv)
        {
            const D3D12_CPU_DESCRIPTOR_HANDLE d3d12DSVDescriptorHandle{ dsv->m_CPUHandle };

            m_D3D12GraphicsCommandList->OMSetRenderTargets(
                static_cast<uint32_t>(d3d12RTVDescriptorHandles.size()),
                d3d12RTVDescriptorHandles.data(),
                isRenderTargetContiguous,
                &d3d12DSVDescriptorHandle
            );
        }
        else
        {
            m_D3D12GraphicsCommandList->OMSetRenderTargets(
                static_cast<uint32_t>(d3d12RTVDescriptorHandles.size()),
                d3d12RTVDescriptorHandles.data(),
                isRenderTargetContiguous,
                nullptr
            );
        }
    }

    void GraphicsCommandList::ClearRenderTarget(const Descriptor& rtv, const TextureResource::ClearColor& clearColor)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12RTVDescriptorHandle{ rtv.GetCPUHandle() };

        m_D3D12GraphicsCommandList->ClearRenderTargetView(d3d12RTVDescriptorHandle, reinterpret_cast<const float*>(&clearColor.Color), 0, nullptr);
    }

    void GraphicsCommandList::ClearDepthStencil(const Descriptor& dsv, const TextureResource::ClearDepthStencil& clearDepthStencil)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12DSVDescriptorHandle{ dsv.GetCPUHandle() };

        m_D3D12GraphicsCommandList->ClearDepthStencilView(
            d3d12DSVDescriptorHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            clearDepthStencil.Depth,
            clearDepthStencil.Stencil,
            0,
            nullptr
        );
    }

    void GraphicsCommandList::DrawVertexed(uint32_t vertexCount, uint32_t instanceCount)
    {
        constexpr uint32_t startVertexLocation = 0;

        m_D3D12GraphicsCommandList->DrawInstanced(vertexCount, instanceCount, startVertexLocation, 0);
    }

    void GraphicsCommandList::DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount)
    {
        m_D3D12GraphicsCommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, 0);
    }

} // namespace benzin
