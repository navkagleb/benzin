#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/command_list.hpp"

// Ref: https://devblogs.microsoft.com/pix/winpixeventruntime/
#define USE_PIX
#include <pix3.h>

#include "benzin/core/asserter.hpp"
#include "benzin/core/math.hpp"
#include "benzin/core/memory_writer.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/descriptor_manager.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/pso.hpp"
#include "benzin/graphics/query_heap.hpp"
#include "benzin/graphics/ray_tracing_acceleration_structures.hpp"
#include "benzin/graphics/ray_tracing_pso.hpp"
#include "benzin/graphics/ray_tracing_shader_table.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/graphics/unified_root_signature.hpp"

namespace benzin
{

    static D3D12_RESOURCE_BARRIER ToD3D12ResourceBarrier(const TransitionBarrier& transitionBarrier)
    {
        return D3D12_RESOURCE_BARRIER
        {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition
            {
                .pResource = transitionBarrier.TransitionResource.GetD3D12Resource(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = (D3D12_RESOURCE_STATES)transitionBarrier.StateBefore,
                .StateAfter = (D3D12_RESOURCE_STATES)transitionBarrier.StateAfter,
            },
        };
    }

    static D3D12_RESOURCE_BARRIER ToD3D12ResourceBarrier(const UnorderedAccessBarrier& unorderedAccessBarrier)
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

    static D3D12_RESOURCE_BARRIER ToD3D12ResourceBarrierVariant(const ResourceBarrierVariant& resourceBarrier)
    {
        return resourceBarrier | MakeVisitorMatch(
            [](const auto& resourceBarrier) { return ToD3D12ResourceBarrier(resourceBarrier); }
        );
    };

    // GraphicsCommandList

    GraphicsCommandList::GraphicsCommandList(Device& device)
    {
        ComPtr<ID3D12GraphicsCommandList1> d3d12GraphicsCommandList1;
        BenzinEnsure(device.GetD3D12Device()->CreateCommandList1(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&d3d12GraphicsCommandList1)
        ));

        BenzinEnsure(d3d12GraphicsCommandList1->QueryInterface(IID_PPV_ARGS(&m_D3D12GraphicsCommandList)));
        SetDxObjectDebugName(m_D3D12GraphicsCommandList, "GraphicsCommandList");
    }

    GraphicsCommandList::~GraphicsCommandList()
    {
        BenzinSafeDxObjectRelease(m_D3D12GraphicsCommandList);
    }

    void GraphicsCommandList::CopyResource(const Resource& destination, const Resource& source)
    {
        m_D3D12GraphicsCommandList->CopyResource(destination.GetD3D12Resource(), source.GetD3D12Resource());
    }

    void GraphicsCommandList::UploadToBuffer(Buffer& buffer, std::span<const std::byte> data, Bytes64 offset)
    {
        BenzinAssert(buffer.GetD3D12Resource());
        BenzinAssert(!data.empty());

        BenzinAssert(m_UploadBuffer->GetD3D12Resource());

        const size_t uploadBufferOffset = AllocateInUploadBuffer(data.size_bytes());

        const MemoryWriter writer{ m_UploadBuffer->GetCpuMappedData(), m_UploadBuffer->GetSize() };
        writer.WriteBytes(data, uploadBufferOffset);

        BenzinMakeScopedResourceBarriers(*this, TransitionBarrier{ buffer, ResourceState::CopyDestination });
        m_D3D12GraphicsCommandList->CopyBufferRegion(
            buffer.GetD3D12Resource(),
            offset,
            m_UploadBuffer->GetD3D12Resource(),
            uploadBufferOffset,
            data.size_bytes()
        );
    }

    void GraphicsCommandList::UploadToTexture(Texture& texture, const std::vector<SubResourceData>& subResources)
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
            Bytes64 resourceSize = 0;

            ComPtr<ID3D12Device> d3d12Device;
            BenzinEnsure(texture.GetD3D12Resource()->GetDevice(IID_PPV_ARGS(&d3d12Device)));

            const D3D12_RESOURCE_DESC d3d12TextureDesc = texture.GetD3D12Resource()->GetDesc();
            const Bytes64 offset = AllocateInUploadBuffer(0, GfxConfig::s_TextureAlignment);

            d3d12Device->GetCopyableFootprints(
                &d3d12TextureDesc,
                firstSubresource,
                (uint32_t)subResources.size(),
                offset,
                copyableFootprits.D3D12Layouts.data(),
                copyableFootprits.RowCounts.data(),
                copyableFootprits.RowSizes.data(),
                &resourceSize
            );

            AllocateInUploadBuffer(resourceSize, GfxConfig::s_TextureAlignment);
        }

        // Copying subresources to UploadBuffer
        // Go down to rows and copy it
        {
            const MemoryWriter writer{ m_UploadBuffer->GetCpuMappedData(), m_UploadBuffer->GetSize() };

            for (size_t subResourceIndex = 0; subResourceIndex < subResources.size(); ++subResourceIndex)
            {
                const SubResourceData& subResource = subResources[subResourceIndex];
                const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& d3d12Layout = copyableFootprits.D3D12Layouts[subResourceIndex];

                // SubResource data
                const Bytes64 destinationOffset = d3d12Layout.Offset;
                const std::byte* sourceData = subResource.Data;

                for (uint32_t sliceIndex = 0; sliceIndex < d3d12Layout.Footprint.Depth; ++sliceIndex)
                {
                    const uint64_t rowCount = copyableFootprits.RowCounts[subResourceIndex];
                    const uint64_t destinationSlicePitch = d3d12Layout.Footprint.RowPitch * rowCount;

                    // Slice data
                    const Bytes64 destinationSliceOffset = destinationOffset + destinationSlicePitch * sliceIndex;
                    const std::byte* sourceSliceData = sourceData + subResource.SlicePitch * sliceIndex;

                    for (uint32_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                    {
                        const Bytes64 destinationRowPitch = d3d12Layout.Footprint.RowPitch;

                        // Row data
                        const Bytes64 destinationRowOffset = destinationSliceOffset + destinationRowPitch * rowIndex;
                        const std::byte* sourceRowData = sourceSliceData + subResource.RowPitch * rowIndex;

                        const Bytes64 rowSize = copyableFootprits.RowSizes[subResourceIndex];
                        writer.WriteBytes(std::span{ sourceRowData, rowSize }, destinationRowOffset);
                    }
                }
            }
        }

        // Copy to texture
        for (size_t i = 0; i < subResources.size(); ++i)
        {
            const D3D12_TEXTURE_COPY_LOCATION destination
            {
                .pResource = texture.GetD3D12Resource(),
                .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                .SubresourceIndex = (uint32_t)i + firstSubresource,
            };

            const D3D12_TEXTURE_COPY_LOCATION source
            {
                .pResource = m_UploadBuffer->GetD3D12Resource(),
                .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                .PlacedFootprint = copyableFootprits.D3D12Layouts[i],
            };

            BenzinMakeScopedResourceBarriers(*this, TransitionBarrier{ texture, ResourceState::CopyDestination });
            m_D3D12GraphicsCommandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        }
    }

    void GraphicsCommandList::UploadToTextureTopMip(Texture& texture, std::span<const std::byte> data)
    {
        const Bytes pixelSize = GetFormatSize(texture.GetFormat());

        const SubResourceData topMipSubResource
        {
            .Data = data.data(),
            .RowPitch = pixelSize * texture.GetWidth(),
            .SlicePitch = pixelSize * texture.GetWidth() * texture.GetHeight(),
        };

        BenzinAssert(topMipSubResource.SlicePitch == data.size_bytes());
        UploadToTexture(texture, { topMipSubResource });
    }

    void GraphicsCommandList::SetRootConstant(uint32_t rootIndex, uint32_t value)
    {
        m_D3D12GraphicsCommandList->SetComputeRoot32BitConstant(+UnifiedRootParameter::RootConstantBuffer, value, rootIndex);
        m_D3D12GraphicsCommandList->SetGraphicsRoot32BitConstant(+UnifiedRootParameter::RootConstantBuffer, value, rootIndex);
    }

    void GraphicsCommandList::SetRootResource(uint32_t rootIndex, const Descriptor& viewDescriptor)
    {
        BenzinAssert(viewDescriptor.IsGpuValid());

        SetRootConstant(rootIndex, viewDescriptor.GetGpuHeapIndex());
    }

    void GraphicsCommandList::SetCbv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)

    void GraphicsCommandList::SetTimestamp(const QueryHeap& timestampQueryHeap, uint32_t index)
    {
        BenzinAssert(timestampQueryHeap.GetD3D12QueryHeap() != nullptr);
        BenzinAssert(index < timestampQueryHeap.GetCount());

        m_D3D12GraphicsCommandList->EndQuery(timestampQueryHeap.GetD3D12QueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, index);
    }

    void GraphicsCommandList::ResolveTimestamps(const QueryHeap& timestampQueryHeap, const Buffer& readbackBuffer, uint64_t readbackBufferOffset)
    {
        BenzinAssert(timestampQueryHeap.GetD3D12QueryHeap() != nullptr);
        BenzinAssert(readbackBuffer.GetD3D12Resource() != nullptr);

        m_D3D12GraphicsCommandList->ResolveQueryData(
            timestampQueryHeap.GetD3D12QueryHeap(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            0,
            timestampQueryHeap.GetCount(),
            readbackBuffer.GetD3D12Resource(),
            readbackBufferOffset
        );
    }

    {
        m_D3D12GraphicsCommandList->SetComputeRootConstantBufferView(+rootParameter, gpuVirtualAddress);
        m_D3D12GraphicsCommandList->SetGraphicsRootConstantBufferView(+rootParameter, gpuVirtualAddress);
    }

    void GraphicsCommandList::SetSrv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList->SetComputeRootShaderResourceView(+rootParameter, gpuVirtualAddress);
        m_D3D12GraphicsCommandList->SetGraphicsRootShaderResourceView(+rootParameter, gpuVirtualAddress);
    }

    void GraphicsCommandList::SetPso(const Pso& pso)
    {
        BenzinAssert(pso.GetD3D12PipelineState());
        m_D3D12GraphicsCommandList->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void GraphicsCommandList::SetPso(const RayTracing_Pso& pso)
    {
        BenzinAssert(pso.GetD3D12StateObject());
        m_D3D12GraphicsCommandList->SetPipelineState1(pso.GetD3D12StateObject());
    }

    void GraphicsCommandList::SetPrimitiveTopology(PrimitiveTopology primitiveTopology)
    {
        BenzinAssert(primitiveTopology != PrimitiveTopology::Unknown);

        m_D3D12GraphicsCommandList->IASetPrimitiveTopology((D3D12_PRIMITIVE_TOPOLOGY)primitiveTopology);
    }

    void GraphicsCommandList::SetViewport(const Viewport& viewport)
    {
        m_D3D12GraphicsCommandList->RSSetViewports(1, reinterpret_cast<const D3D12_VIEWPORT*>(&viewport));
    }

    void GraphicsCommandList::SetScissorRect(const ScissorRect& scissorRect)
    {
        const D3D12_RECT d3d12Rect
        {
            .left = (LONG)scissorRect.X,
            .top = (LONG)scissorRect.Y,
            .right = (LONG)(scissorRect.X + scissorRect.Width),
            .bottom = (LONG)(scissorRect.Y + scissorRect.Height),
        };

        m_D3D12GraphicsCommandList->RSSetScissorRects(1, &d3d12Rect);
    }

    void GraphicsCommandList::SetRenderTargets(const std::vector<Descriptor>& rtvs, const Descriptor* dsv)
    {
        constexpr bool isRenderTargetContiguous = false;

        BenzinAssert(rtvs.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> d3d12RtvDescriptorHandles;
        d3d12RtvDescriptorHandles.reserve(rtvs.size());

        for (const auto& rtv : rtvs)
        {
            BenzinAssert(rtv.GetType() == DescriptorType::Rtv);
            d3d12RtvDescriptorHandles.emplace_back(rtv.GetCpuHandle());
        }

        if (dsv)
        {
            BenzinAssert(dsv->GetType() == DescriptorType::Dsv);
            const D3D12_CPU_DESCRIPTOR_HANDLE d3d12DsvDescriptorHandle{ dsv->GetCpuHandle() };

            m_D3D12GraphicsCommandList->OMSetRenderTargets(
                (uint32_t)d3d12RtvDescriptorHandles.size(),
                d3d12RtvDescriptorHandles.data(),
                isRenderTargetContiguous,
                &d3d12DsvDescriptorHandle
            );
        }
        else
        {
            m_D3D12GraphicsCommandList->OMSetRenderTargets(
                (uint32_t)d3d12RtvDescriptorHandles.size(),
                d3d12RtvDescriptorHandles.data(),
                isRenderTargetContiguous,
                nullptr
            );
        }
    }

    void GraphicsCommandList::ClearRenderTarget(const Texture& renderTarget)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12RtvDescriptorHandle{ renderTarget.GetRtv().GetCpuHandle() };
        const auto& clearColor = renderTarget.GetClearColor();

        m_D3D12GraphicsCommandList->ClearRenderTargetView(d3d12RtvDescriptorHandle, reinterpret_cast<const float*>(&clearColor), 0, nullptr);
    }

    void GraphicsCommandList::ClearDepthStencil(const Texture& depthStencil)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12DsvDescriptorHandle{ depthStencil.GetDsv().GetCpuHandle() };
        const auto clearDepthStencil = depthStencil.GetClearDepthStencil();

        m_D3D12GraphicsCommandList->ClearDepthStencilView(
            d3d12DsvDescriptorHandle,
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

    void GraphicsCommandList::ClearUnorderedAccess(const Texture& unorderedAccess, const DirectX::XMFLOAT4& color)
    {
        BenzinUnused(unorderedAccess);
        BenzinUnused(color);

        // For now supported only default uavs

        const auto& uav = unorderedAccess.GetUav();

        m_D3D12GraphicsCommandList->ClearUnorderedAccessViewFloat(
            D3D12_GPU_DESCRIPTOR_HANDLE{ uav.GetGpuHandle() },
            D3D12_CPU_DESCRIPTOR_HANDLE{ uav.GetCpuHandle() },
            unorderedAccess.GetD3D12Resource(),
            (const float*)&color,
            0,
            nullptr // Clears entire texture
        );
    }

    void GraphicsCommandList::Dispatch(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadGroupSize)
    {
        BenzinAssert(dimension.x != 0 && dimension.y != 0 && dimension.z != 0);
        BenzinAssert(threadGroupSize.x != 0 && threadGroupSize.y != 0 && threadGroupSize.z != 0);

        const DirectX::XMUINT3 threadGroupCount
        {
            std::max(DivideUp(dimension.x, threadGroupSize.x), 1u),
            std::max(DivideUp(dimension.y, threadGroupSize.y), 1u),
            std::max(DivideUp(dimension.z, threadGroupSize.z), 1u),
        };

        m_D3D12GraphicsCommandList->Dispatch(threadGroupCount.x, threadGroupCount.y, threadGroupCount.z);
    }

    void GraphicsCommandList::BuildRayTracingAccelerationStructure(const RayTracing_AcclerationStructure& accelerationStructure)
    {
        BenzinAssert(accelerationStructure.GetScratchResource()->GetCurrentState() == ResourceState::UnorderedAccess);

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC d3d12BuildAccelerationStructureDesc
        {
            .DestAccelerationStructureData = accelerationStructure.GetBuffer()->GetGpuVirtualAddress(),
            .Inputs = accelerationStructure.GetD3D12BuildInputs(),
            .SourceAccelerationStructureData = 0,
            .ScratchAccelerationStructureData = accelerationStructure.GetScratchResource()->GetGpuVirtualAddress(),
        };

        m_D3D12GraphicsCommandList->BuildRaytracingAccelerationStructure(&d3d12BuildAccelerationStructureDesc, 0, nullptr);
    }

    void GraphicsCommandList::DispatchRays(const RayTracing_ShaderTable& shaderTable, const DirectX::XMUINT3 dimenions)
    {
        BenzinAssert(dimenions.x != 0 && dimenions.y != 0 && dimenions.z != 0);

        const RayTracing_ShaderTable::GpuAddresses& gpuAddresses = shaderTable.GetGpuAddresses();

        const D3D12_DISPATCH_RAYS_DESC d3d12DispatchRayDesc
        {
            .RayGenerationShaderRecord
            {
                .StartAddress = gpuAddresses.RayGenerationShader.GpuVirtualAddress,
                .SizeInBytes = gpuAddresses.RayGenerationShader.Size,
            },
            .MissShaderTable
            {
                .StartAddress = gpuAddresses.MissTable.GpuVirtualAddress,
                .SizeInBytes = gpuAddresses.MissTable.Size,
                .StrideInBytes = 0, // TODO: For now supported only one record per table
            },
            .HitGroupTable
            {
                .StartAddress = gpuAddresses.HitGroupTable.GpuVirtualAddress,
                .SizeInBytes = gpuAddresses.MissTable.Size,
                .StrideInBytes = 0, // TODO: For now supported only one record per table
            },
            .CallableShaderTable
            {
                .StartAddress = 0,
                .SizeInBytes = 0,
                .StrideInBytes = 0,
            },
            .Width = dimenions.x,
            .Height = dimenions.y,
            .Depth = dimenions.z,
        };

        m_D3D12GraphicsCommandList->DispatchRays(&d3d12DispatchRayDesc);
    }

    void GraphicsCommandList::SetUploadBuffer(Buffer& uploadBuffer)
    {
        m_UploadBuffer = &uploadBuffer;
        m_UploadBufferOffset = 0;
    }

    Bytes64 GraphicsCommandList::AllocateInUploadBuffer(Bytes64 size, Bytes64 alignment)
    {
        BenzinEnsure(m_UploadBuffer != nullptr);

        Bytes64 alignedOffset = m_UploadBufferOffset;
        if (alignment != 0)
        {
            alignedOffset = AlignUp(m_UploadBufferOffset.GetByteCount(), alignment.GetByteCount());
        }

        m_UploadBufferOffset = alignedOffset + size;
        BenzinEnsure(m_UploadBufferOffset <= m_UploadBuffer->GetSize());

        return alignedOffset;
    }

    // ResourceBarriers

    ResourceBarriers::ResourceBarriers(GraphicsCommandList& commandList, const std::vector<ResourceBarrierVariant>& resourceBarriers, bool isScoped)
        : m_CommandList{ commandList }
        , m_IsScoped{ isScoped }
    {
        std::vector<D3D12_RESOURCE_BARRIER> d3d12Barriers;

        for (const auto& barrierVariant : resourceBarriers)
        {
            const auto* transitionBarrier = std::get_if<TransitionBarrier>(&barrierVariant);
            if (transitionBarrier != nullptr)
            {
                transitionBarrier->TransitionResource.SetCurrentState(transitionBarrier->StateAfter);

                if (m_IsScoped)
                {
                    m_SwappedTransitionBarriers.push_back(*transitionBarrier);
                }
            }

            d3d12Barriers.push_back(ToD3D12ResourceBarrierVariant(barrierVariant));
        }

        SetD3D12Barriers(d3d12Barriers);
    }

    ResourceBarriers::~ResourceBarriers()
    {
        if (!m_IsScoped || m_SwappedTransitionBarriers.empty())
        {
            return;
        }

        for (auto& transitionBarrier : m_SwappedTransitionBarriers)
        {
            std::swap(transitionBarrier.StateAfter, transitionBarrier.StateBefore);
            transitionBarrier.TransitionResource.SetCurrentState(transitionBarrier.StateAfter);
        }

        const auto d3d12Barriers = m_SwappedTransitionBarriers | std::views::transform(ToD3D12ResourceBarrierVariant) | std::ranges::to<std::vector>();
        SetD3D12Barriers(d3d12Barriers);
    }

    void ResourceBarriers::SetD3D12Barriers(std::span<const D3D12_RESOURCE_BARRIER> d3d12Barriers) const
    {
        if (d3d12Barriers.empty())
        {
            return;
        }

        auto* d3d12CommandList = m_CommandList.GetD3D12GraphicsCommandList();
        d3d12CommandList->ResourceBarrier((uint32_t)d3d12Barriers.size(), d3d12Barriers.data());
    }

    // ScopedGpuEvent

    ScopedGpuEvent::ScopedGpuEvent(GraphicsCommandList& commandList, std::string_view name)
        : m_D3D12GraphicsCommandList{ commandList.GetD3D12GraphicsCommandList() }
    {
        PIXBeginEvent(m_D3D12GraphicsCommandList, PIX_COLOR_DEFAULT, "%s", name.data());
    }

    ScopedGpuEvent::~ScopedGpuEvent()
    {
        PIXEndEvent(m_D3D12GraphicsCommandList);
    }

}
