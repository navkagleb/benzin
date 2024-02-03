#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/command_list.hpp"

#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/descriptor_manager.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/mapped_data.hpp"
#include "benzin/graphics/pipeline_state.hpp"
#include "benzin/graphics/rt_acceleration_structures.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    namespace
    {

        D3D12_RESOURCE_BARRIER ToD3D12ResourceBarrier(const TransitionBarrier& transitionBarrier)
        {
            return D3D12_RESOURCE_BARRIER
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition
                {
                    .pResource = transitionBarrier.Resource.GetD3D12Resource(),
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = (D3D12_RESOURCE_STATES)transitionBarrier.Resource.GetCurrentState(),
                    .StateAfter = (D3D12_RESOURCE_STATES)transitionBarrier.StateAfter,
                },
            };
        }

        D3D12_RESOURCE_BARRIER ToD3D12ResourceBarrier(const UnorderedAccessBarrier& unorderedAccessBarrier)
        {
            return D3D12_RESOURCE_BARRIER
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .UAV
                {
                    .pResource = unorderedAccessBarrier.Resource.GetD3D12Resource(),
                },
            };
        }

        D3D12_RESOURCE_BARRIER ToD3D12ResourceBarrierVariant(const ResourceBarrierVariant& resourceBarrier)
        {
            return std::visit([](auto&& resourceBarrier) { return ToD3D12ResourceBarrier(resourceBarrier); }, resourceBarrier);
        };

    } // anonymous namespace

    // CommandList
    CommandList::CommandList(Device& device, CommandListType commandListType)
    {
        BenzinAssert(device.GetD3D12Device());

        ComPtr<ID3D12GraphicsCommandList1> d3d12GraphicsCommandList1;
        BenzinAssert(device.GetD3D12Device()->CreateCommandList1(
            0,
            static_cast<D3D12_COMMAND_LIST_TYPE>(commandListType),
            D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&d3d12GraphicsCommandList1)
        ));

        BenzinAssert(d3d12GraphicsCommandList1->QueryInterface(IID_PPV_ARGS(&m_D3D12GraphicsCommandList)));

        SetD3D12ObjectDebugName(m_D3D12GraphicsCommandList, magic_enum::enum_name((D3D12_COMMAND_LIST_TYPE)commandListType));
    }

    CommandList::~CommandList()
    {
        SafeUnknownRelease(m_D3D12GraphicsCommandList);
    }

    void CommandList::SetResourceBarrier(const ResourceBarrierVariant& resourceBarrier)
    {
        const D3D12_RESOURCE_BARRIER d3d12ResourceBarrier = ToD3D12ResourceBarrierVariant(resourceBarrier);
        m_D3D12GraphicsCommandList->ResourceBarrier(1, &d3d12ResourceBarrier);

        if (const auto* transitionBarrier = std::get_if<TransitionBarrier>(&resourceBarrier))
        {
            transitionBarrier->Resource.SetCurrentState(transitionBarrier->StateAfter);
        }
    }

    void CommandList::SetResourceBarriers(const std::vector<ResourceBarrierVariant>& resourceBarriers)
    {
        const auto d3d12ResourceBarriers = resourceBarriers | std::views::transform(ToD3D12ResourceBarrierVariant) | std::ranges::to<std::vector>();
        m_D3D12GraphicsCommandList->ResourceBarrier((uint32_t)d3d12ResourceBarriers.size(), d3d12ResourceBarriers.data());

        for (const auto& resourceBarrier : resourceBarriers)
        {
            if (const auto* transitionBarrier = std::get_if<TransitionBarrier>(&resourceBarrier))
            {
                transitionBarrier->Resource.SetCurrentState(transitionBarrier->StateAfter);
            }
        }
    }

    void CommandList::CopyResource(Resource& to, Resource& from)
    {
        m_D3D12GraphicsCommandList->CopyResource(to.GetD3D12Resource(), from.GetD3D12Resource());
    }

    // CopyCommandList
    CopyCommandList::CopyCommandList(Device& device)
        : CommandList{ device, CommandListType::Copy }
    {}

    CopyCommandList::~CopyCommandList() = default;

    void CopyCommandList::UpdateBuffer(Buffer& buffer, std::span<const std::byte> data, size_t offsetInBytes)
    {
        BenzinAssert(buffer.GetD3D12Resource());
        BenzinAssert(!data.empty());

        BenzinAssert(m_UploadBuffer->GetD3D12Resource());

        const size_t uploadBufferOffset = AllocateInUploadBuffer(data.size_bytes());

        {
            MappedData uploadBuffer{ *m_UploadBuffer };
            uploadBuffer.Write(data, uploadBufferOffset);
        }

        m_D3D12GraphicsCommandList->CopyBufferRegion(
            buffer.GetD3D12Resource(),
            offsetInBytes,
            m_UploadBuffer->GetD3D12Resource(),
            uploadBufferOffset,
            data.size_bytes()
        );
    }

    void CopyCommandList::UpdateTexture(Texture& texture, const std::vector<SubResourceData>& subResources)
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

        BenzinAssert(texture.GetD3D12Resource());
        BenzinAssert(!subResources.empty());

        constexpr uint32_t firstSubresource = 0;

        CopyableFootprints copyableFootprits{ subResources.size() };

        // Init CopyableFootprints and allocate memory in UploadBuffer
        {
            uint64_t resourceSize = 0;

            {
                ComPtr<ID3D12Device> d3d12Device;
                texture.GetD3D12Resource()->GetDevice(IID_PPV_ARGS(&d3d12Device));

                const D3D12_RESOURCE_DESC d3d12TextureDesc = texture.GetD3D12Resource()->GetDesc();
                const size_t offset = AllocateInUploadBuffer(0, config::g_TextureAlignment);

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

            AllocateInUploadBuffer(resourceSize, config::g_TextureAlignment);
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

        //SetResourceBarrier(texture, ResourceState::CopyDestination);

        // Copy to texture
        for (size_t i = 0; i < subResources.size(); ++i)
        {
            const D3D12_TEXTURE_COPY_LOCATION destination
            {
                .pResource = texture.GetD3D12Resource(),
                .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                .SubresourceIndex = static_cast<uint32_t>(i + firstSubresource),
            };

            const D3D12_TEXTURE_COPY_LOCATION source
            {
                .pResource = m_UploadBuffer->GetD3D12Resource(),
                .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                .PlacedFootprint = copyableFootprits.D3D12Layouts[i],
            };

            m_D3D12GraphicsCommandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        }

        //SetResourceBarrier(texture, ResourceState::Present);
    }

    void CopyCommandList::UpdateTextureTopMip(Texture& texture, std::span<const std::byte> data)
    {
        const uint32_t pixelSizeInBytes = GetFormatSizeInBytes(texture.GetFormat());

        const SubResourceData topMipSubResource
        {
            .Data = data.data(),
            .RowPitch = pixelSizeInBytes * texture.GetWidth(),
            .SlicePitch = pixelSizeInBytes * texture.GetWidth() * texture.GetHeight(),
        };

        BenzinAssert(topMipSubResource.SlicePitch == data.size_bytes());
        UpdateTexture(texture, { topMipSubResource });
    }

    void CopyCommandList::CreateUploadBuffer(Device& device, uint32_t size)
    {
        m_UploadBuffer = std::make_unique<Buffer>(device, BufferCreation
        {
            .ElementCount = size,
            .Flags = BufferFlag::UploadBuffer,
        });
    }

    void CopyCommandList::ReleaseUploadBuffer()
    {
        m_UploadBuffer.release();
        m_UploadBufferOffset = 0;
    }

    size_t CopyCommandList::AllocateInUploadBuffer(size_t size, size_t alignment)
    {
        const size_t alignedOffset = alignment == 0 ? m_UploadBufferOffset : AlignAbove(m_UploadBufferOffset, alignment);

        m_UploadBufferOffset = alignedOffset + size;
        BenzinAssert(alignedOffset + size <= m_UploadBuffer->GetSizeInBytes());

        return alignedOffset;
    }

    // ComputeCommandList
    ComputeCommandList::ComputeCommandList(Device& device)
        : CommandList{ device, CommandListType::Compute }
    {}

    void ComputeCommandList::SetRootConstant(uint32_t rootIndex, uint32_t value)
    {
        const uint32_t rootParameterIndex = 0;

        m_D3D12GraphicsCommandList->SetComputeRoot32BitConstant(rootParameterIndex, value, rootIndex);
    }

    void ComputeCommandList::SetRootResource(uint32_t rootIndex, const Descriptor& viewDescriptor)
    {
        BenzinAssert(viewDescriptor.IsValid());

        SetRootConstant(rootIndex, viewDescriptor.GetHeapIndex());
    }

    void ComputeCommandList::SetPipelineState(const PipelineState& pso)
    {
        BenzinAssert(pso.GetD3D12PipelineState());

        m_D3D12GraphicsCommandList->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void ComputeCommandList::Dispatch(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadPerGroupCount)
    {
        BenzinAssert(threadPerGroupCount.x != 0);
        BenzinAssert(threadPerGroupCount.y != 0);
        BenzinAssert(threadPerGroupCount.z != 0);

        const uint32_t groupCountX = AlignThreadGroupCount(dimension.x, threadPerGroupCount.x);
        const uint32_t groupCountY = AlignThreadGroupCount(dimension.y, threadPerGroupCount.y);
        const uint32_t groupCountZ = AlignThreadGroupCount(dimension.z, threadPerGroupCount.z);

        m_D3D12GraphicsCommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
    }

    // GraphicsCommandList
	GraphicsCommandList::GraphicsCommandList(Device& device)
        : CommandList{ device, CommandListType::Direct }
    {}

    void GraphicsCommandList::SetRootConstant(uint32_t rootIndex, uint32_t value)
    {
        const uint32_t rootParameterIndex = 0;

        m_D3D12GraphicsCommandList->SetComputeRoot32BitConstant(rootParameterIndex, value, rootIndex);
        m_D3D12GraphicsCommandList->SetGraphicsRoot32BitConstant(rootParameterIndex, value, rootIndex);
    }

    void GraphicsCommandList::SetRootResource(uint32_t rootIndex, const Descriptor& viewDescriptor)
    {
        BenzinAssert(viewDescriptor.IsValid());

        SetRootConstant(rootIndex, viewDescriptor.GetHeapIndex());
    }

    void GraphicsCommandList::SetPipelineState(const PipelineState& pso)
    {
        BenzinAssert(pso.GetD3D12PipelineState());

        m_D3D12GraphicsCommandList->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void GraphicsCommandList::SetPrimitiveTopology(PrimitiveTopology primitiveTopology)
    {
        BenzinAssert(primitiveTopology != PrimitiveTopology::Unknown);

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
            .left = static_cast<LONG>(scissorRect.X),
            .top = static_cast<LONG>(scissorRect.Y),
            .right = static_cast<LONG>(scissorRect.X + scissorRect.Width),
            .bottom = static_cast<LONG>(scissorRect.Y + scissorRect.Height),
        };

        m_D3D12GraphicsCommandList->RSSetScissorRects(1, &d3d12Rect);
    }

    void GraphicsCommandList::SetRenderTargets(const std::vector<Descriptor>& rtvs, const Descriptor* dsv)
    {
        constexpr bool isRenderTargetContiguous = false;

        BenzinAssert(rtvs.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> d3d12RTVDescriptorHandles;
        d3d12RTVDescriptorHandles.reserve(rtvs.size());

        for (const Descriptor& descriptor : rtvs)
        {
            d3d12RTVDescriptorHandles.emplace_back(descriptor.GetCPUHandle());
        }

        if (dsv)
        {
            const D3D12_CPU_DESCRIPTOR_HANDLE d3d12DSVDescriptorHandle{ dsv->GetCPUHandle() };

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

    void GraphicsCommandList::ClearRenderTarget(const Descriptor& rtv, const DirectX::XMFLOAT4& color)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12RTVDescriptorHandle{ rtv.GetCPUHandle() };

        m_D3D12GraphicsCommandList->ClearRenderTargetView(d3d12RTVDescriptorHandle, reinterpret_cast<const float*>(&color), 0, nullptr);
    }

    void GraphicsCommandList::ClearDepthStencil(const Descriptor& dsv, const DepthStencil& depthStencil)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12DSVDescriptorHandle{ dsv.GetCPUHandle() };

        m_D3D12GraphicsCommandList->ClearDepthStencilView(
            d3d12DSVDescriptorHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            depthStencil.Depth,
            depthStencil.Stencil,
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

    void GraphicsCommandList::BuildRayTracingAccelerationStructure(const rt::AccelerationStructure& accelerationStructure)
    {
        BenzinAssert(accelerationStructure.GetScratchResource().GetCurrentState() == ResourceState::UnorderedAccess);

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC d3d12BuildAccelerationStructureDesc
        {
            .DestAccelerationStructureData = accelerationStructure.GetBuffer().GetGPUVirtualAddress(),
            .Inputs = accelerationStructure.GetD3D12BuildInputs(),
            .SourceAccelerationStructureData = 0,
            .ScratchAccelerationStructureData = accelerationStructure.GetScratchResource().GetGPUVirtualAddress(),
        };

        m_D3D12GraphicsCommandList->BuildRaytracingAccelerationStructure(&d3d12BuildAccelerationStructureDesc, 0, nullptr);
    }

} // namespace benzin
