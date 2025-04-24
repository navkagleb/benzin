#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/cmd_list.hpp>

// Ref: https://devblogs.microsoft.com/pix/winpixeventruntime/
#define USE_PIX
#include <pix3.h>

#include <benzin/core/math.hpp>
#include <benzin/core/memory_writer.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/descriptor_manager.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/pso.hpp>
#include <benzin/graphics/query_heap.hpp>
#include <benzin/graphics/ray_tracing_acceleration_structures.hpp>
#include <benzin/graphics/ray_tracing_pso.hpp>
#include <benzin/graphics/ray_tracing_shader_table.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

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

    // CmdList

    CmdList::CmdList(Device& device)
    {
        BenzinD3D12Call(device.GetD3D12Device()->CreateCommandList1(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT, // TODO: Support more cmd list types
            D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&m_D3D12GraphicsCommandList1)
        ));

        SetD3DObjectDebugName(m_D3D12GraphicsCommandList1, "GraphicsCmdList");
    }

    CmdList::~CmdList()
    {
        SafeReleaseD3DObject(m_D3D12GraphicsCommandList1);
    }

    void CmdList::AddResourceBarrier(const ResourceBarrierVariant& resourceBarrierVariant, bool isNeedToFlush)
    {
        const auto* transitionBarrier = std::get_if<TransitionBarrier>(&resourceBarrierVariant);
        if (transitionBarrier != nullptr)
        {
            transitionBarrier->TransitionResource.SetCurrentState(transitionBarrier->StateAfter);
        }

        m_D3D12Barriers.push_back(ToD3D12ResourceBarrierVariant(resourceBarrierVariant));

        if (isNeedToFlush)
        {
            FlushResourceBarriers();
        }
    }

    void CmdList::FlushResourceBarriers()
    {
        BenzinAssert(!m_D3D12Barriers.empty());

        m_D3D12GraphicsCommandList1->ResourceBarrier((uint32_t)m_D3D12Barriers.size(), m_D3D12Barriers.data());
        m_D3D12Barriers.clear();
    }

    // CopyCmdList

    void CopyCmdList::CopyResource(const Resource& destResource, const Resource& sourceResource)
    {
        BenzinAssert(
            (dynamic_cast<const Buffer*>(&destResource) != nullptr && dynamic_cast<const Buffer*>(&sourceResource) != nullptr) ||
            (dynamic_cast<const Texture*>(&destResource) != nullptr && dynamic_cast<const Texture*>(&sourceResource) != nullptr)
        );

        BenzinAssert(destResource.GetD3D12Resource() != nullptr && sourceResource.GetD3D12Resource() != nullptr);

        m_D3D12GraphicsCommandList1->CopyResource(destResource.GetD3D12Resource(), sourceResource.GetD3D12Resource());
    }

    void CopyCmdList::CopyBufferRegion(const Buffer& destBuffer, uint64_t destOffsetInBytes, const Buffer& sourceBuffer, uint64_t sourceOffsetInBytes, uint64_t dataSizeInBytes)
    {
        BenzinAssert(destBuffer.GetD3D12Resource() != nullptr);
        BenzinAssert(sourceBuffer.GetD3D12Resource() != nullptr);
        BenzinAssert(dataSizeInBytes != 0);

        BenzinScopedResourceBarriers(
            *this,
            TransitionBarrier{ destBuffer, ResourceState::CopyDestination },
            TransitionBarrier{ sourceBuffer, ResourceState::CopySource },
        );

        m_D3D12GraphicsCommandList1->CopyBufferRegion(
            destBuffer.GetD3D12Resource(),
            destOffsetInBytes,
            sourceBuffer.GetD3D12Resource(),
            sourceOffsetInBytes,
            dataSizeInBytes
        );
    }

    void CopyCmdList::CopyTextureRegion(const Texture& destTexture, uint32_t destSubResourceIndex, const Texture& sourceTexture, uint32_t sourceSubresourceIndex)
    {
        BenzinAssert(destTexture.GetD3D12Resource() != nullptr && sourceTexture.GetD3D12Resource() != nullptr);
        BenzinAssert(destTexture.GetFormat() == sourceTexture.GetFormat());

        const D3D12_TEXTURE_COPY_LOCATION d3d12DestLocatiton
        {
            .pResource = destTexture.GetD3D12Resource(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = destSubResourceIndex,
        };

        const D3D12_TEXTURE_COPY_LOCATION d3d12SourceLocatiton
        {
            .pResource = sourceTexture.GetD3D12Resource(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = sourceSubresourceIndex,
        };

        BenzinScopedResourceBarriers(
            *this,
            TransitionBarrier{ destTexture, ResourceState::CopyDestination },
            TransitionBarrier{ sourceTexture, ResourceState::CopySource }
        );

        m_D3D12GraphicsCommandList1->CopyTextureRegion(&d3d12DestLocatiton, 0, 0, 0, &d3d12SourceLocatiton, nullptr);
    }

    void CopyCmdList::UploadToBuffer(Buffer& destBuffer, std::span<const std::byte> data, uint64_t destOffsetInBytes)
    {
        BenzinAssert(m_UploadBuffer != nullptr);
        BenzinAssert(!data.empty());

        const uint64_t uploadOffsetInBytes = AllocateInUploadBuffer(data.size_bytes());

        const MemoryWriter writer{ m_UploadBuffer->GetCpuMappedData(), m_UploadBuffer->GetSize() };
        writer.WriteBytes(data, uploadOffsetInBytes);

        CopyBufferRegion(destBuffer, destOffsetInBytes, *m_UploadBuffer, uploadOffsetInBytes, data.size_bytes());
    }

    void CopyCmdList::UploadToTexture(Texture& texture, const std::vector<SubResourceData>& subResources)
    {
        struct CopyableFootprints
        {
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> D3D12Layouts; // RowPitch aligned by D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
            std::vector<uint32_t> RowCounts;
            std::vector<uint64_t> RowSizesInBytes;

            CopyableFootprints(size_t size)
            {
                D3D12Layouts.resize(size);
                RowCounts.resize(size);
                RowSizesInBytes.resize(size);
            }
        };

        BenzinAssert(texture.GetD3D12Resource() != nullptr);
        BenzinAssert(!subResources.empty());

        CopyableFootprints copyableFootprits{ subResources.size() };

        // Init CopyableFootprints and allocate memory in UploadBuffer
        {
            uint64_t resourceSizeInBytes = 0;

            ComPtr<ID3D12Device> d3d12Device;
            BenzinD3D12Call(texture.GetD3D12Resource()->GetDevice(IID_PPV_ARGS(&d3d12Device)));

            const D3D12_RESOURCE_DESC d3d12TextureDesc = texture.GetD3D12Resource()->GetDesc();
            const uint64_t offsetInBytes = AllocateInUploadBuffer(0, GfxConfig::s_TextureAlignment);

            d3d12Device->GetCopyableFootprints(
                &d3d12TextureDesc,
                0, // first sub resource index
                (uint32_t)subResources.size(),
                offsetInBytes,
                copyableFootprits.D3D12Layouts.data(),
                copyableFootprits.RowCounts.data(),
                copyableFootprits.RowSizesInBytes.data(),
                &resourceSizeInBytes
            );

            AllocateInUploadBuffer(resourceSizeInBytes, GfxConfig::s_TextureAlignment);
        }

        // Copying sub-resources to UploadBuffer
        // Go down to rows and copy it
        {
            const MemoryWriter writer{ m_UploadBuffer->GetCpuMappedData(), m_UploadBuffer->GetSize() };

            for (uint32_t subResourceIndex = 0; subResourceIndex < (uint32_t)subResources.size(); ++subResourceIndex)
            {
                const SubResourceData& subResource = subResources[subResourceIndex];
                const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& d3d12Layout = copyableFootprits.D3D12Layouts[subResourceIndex];

                // SubResource data
                const uint64_t destOffsetInBytes = d3d12Layout.Offset;
                const std::byte* sourceData = subResource.Data;

                for (uint32_t sliceIndex = 0; sliceIndex < d3d12Layout.Footprint.Depth; ++sliceIndex)
                {
                    const uint64_t rowCount = copyableFootprits.RowCounts[subResourceIndex];
                    const uint64_t destSlicePitchInBytes = d3d12Layout.Footprint.RowPitch * rowCount;

                    // Slice data
                    const uint64_t destSliceOffsetInBytes = destOffsetInBytes + destSlicePitchInBytes * sliceIndex;
                    const std::byte* sourceSliceData = sourceData + subResource.SlicePitchInBytes * sliceIndex;

                    for (uint32_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                    {
                        const uint64_t destRowPitchInBytes = d3d12Layout.Footprint.RowPitch;

                        // Row data
                        const uint64_t destRowOffsetInBytes = destSliceOffsetInBytes + destRowPitchInBytes * rowIndex;
                        const std::byte* sourceRowData = sourceSliceData + subResource.RowPitchInBytes * rowIndex;

                        const uint64_t rowSizeInBytes = copyableFootprits.RowSizesInBytes[subResourceIndex];
                        writer.WriteBytes(std::span{ sourceRowData, rowSizeInBytes }, destRowOffsetInBytes);
                    }
                }
            }
        }

        // Copy to texture
        for (uint32_t i = 0; i < (uint32_t)subResources.size(); ++i)
        {
            const D3D12_TEXTURE_COPY_LOCATION destination
            {
                .pResource = texture.GetD3D12Resource(),
                .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                .SubresourceIndex = i,
            };

            const D3D12_TEXTURE_COPY_LOCATION source
            {
                .pResource = m_UploadBuffer->GetD3D12Resource(),
                .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                .PlacedFootprint = copyableFootprits.D3D12Layouts[i],
            };

            BenzinScopedResourceBarriers(*this, TransitionBarrier{ texture, ResourceState::CopyDestination });
            m_D3D12GraphicsCommandList1->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        }
    }

    void CopyCmdList::UploadToTexture(Texture& texture, std::span<const std::byte> data)
    {
        BenzinAssert(texture.GetMipCount() == 1);

        const uint32_t pixelSizeInBytes = GetFormatSize(texture.GetFormat());
        const uint64_t rowPitchInBytes = pixelSizeInBytes * texture.GetWidth();
        const uint64_t slicePitchInBytes = rowPitchInBytes * texture.GetHeight();

        BenzinAssert(slicePitchInBytes * texture.GetDepth() == data.size_bytes());

        std::vector<SubResourceData> subResources;
        subResources.reserve(texture.GetDepth());

        for (uint16_t depthIndex = 0; depthIndex < texture.GetDepth(); ++depthIndex)
        {
            subResources.push_back(SubResourceData
            {
                .Data = data.data() + slicePitchInBytes * depthIndex,
                .RowPitchInBytes = rowPitchInBytes,
                .SlicePitchInBytes = slicePitchInBytes,
            });
        }

        UploadToTexture(texture, subResources);
    }

    void CopyCmdList::SetUploadBuffer(Buffer& uploadBuffer)
    {
        m_UploadBuffer = &uploadBuffer;
        m_UploadBufferOffsetInBytes = 0;
    }

    uint64_t CopyCmdList::AllocateInUploadBuffer(uint64_t sizeInBytes, uint64_t alignmentInBytes)
    {
        BenzinEnsure(m_UploadBuffer != nullptr);

        uint64_t alignedOffsetInBytes = m_UploadBufferOffsetInBytes;
        if (alignmentInBytes != 0)
        {
            alignedOffsetInBytes = AlignUp(m_UploadBufferOffsetInBytes, alignmentInBytes);
        }

        m_UploadBufferOffsetInBytes = alignedOffsetInBytes + sizeInBytes;
        BenzinEnsure(m_UploadBufferOffsetInBytes <= m_UploadBuffer->GetSize());

        return alignedOffsetInBytes;
    }

    // ComputeCmdList

    ComputeCmdList::ComputeCmdList(Device& device)
        : CopyCmdList{ device }
    {
        BenzinD3D12Call(m_D3D12GraphicsCommandList1->QueryInterface(IID_PPV_ARGS(&m_D3D12GraphicsCommandList4)));
    }

    ComputeCmdList::~ComputeCmdList()
    {
        SafeReleaseD3DObject(m_D3D12GraphicsCommandList4, false);
    }

    void ComputeCmdList::SetTimestamp(const QueryHeap& timestampQueryHeap, uint32_t index)
    {
        BenzinAssert(timestampQueryHeap.GetD3D12QueryHeap() != nullptr);
        BenzinAssert(index < timestampQueryHeap.GetCount());

        m_D3D12GraphicsCommandList1->EndQuery(timestampQueryHeap.GetD3D12QueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, index);
    }

    void ComputeCmdList::ResolveTimestamps(const QueryHeap& timestampQueryHeap, const Buffer& readbackBuffer, uint64_t readbackOffsetInBytes)
    {
        BenzinAssert(timestampQueryHeap.GetD3D12QueryHeap() != nullptr);
        BenzinAssert(readbackBuffer.GetD3D12Resource() != nullptr && readbackBuffer.GetMemoryType() == ResourceMemoryType::Readback);

        m_D3D12GraphicsCommandList1->ResolveQueryData(
            timestampQueryHeap.GetD3D12QueryHeap(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            0,
            timestampQueryHeap.GetCount(),
            readbackBuffer.GetD3D12Resource(),
            readbackOffsetInBytes
        );
    }

    void ComputeCmdList::SetComputeCbv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList1->SetComputeRootConstantBufferView(+rootParameter, gpuVirtualAddress);
    }

    void ComputeCmdList::SetComputeSrv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList1->SetComputeRootShaderResourceView(+rootParameter, gpuVirtualAddress);
    }

    void ComputeCmdList::SetComputeRootConstant(uint32_t rootIndex, uint32_t value)
    {
        m_D3D12GraphicsCommandList1->SetComputeRoot32BitConstant(+UnifiedRootParameter::Root32Consts, value, rootIndex);
    }

    void ComputeCmdList::SetComputeRootResource(uint32_t rootIndex, const Descriptor& viewDescriptor)
    {
        BenzinAssert(viewDescriptor.IsGpuValid());
        SetComputeRootConstant(rootIndex, viewDescriptor.GetGpuHeapIndex());
    }

    void ComputeCmdList::SetComputePso(const ComputePso& pso)
    {
        BenzinAssert(pso.GetD3D12PipelineState() != nullptr);
        m_D3D12GraphicsCommandList1->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void ComputeCmdList::ClearUnorderedAccess(const Resource& resource, const Descriptor& uav, const DirectX::XMFLOAT4& color)
    {
        BenzinAssert(uav.IsCpuValid());
        BenzinAssert(uav.IsGpuValid());

        m_D3D12GraphicsCommandList1->ClearUnorderedAccessViewFloat(
            D3D12_GPU_DESCRIPTOR_HANDLE{ uav.GetGpuHandle() },
            D3D12_CPU_DESCRIPTOR_HANDLE{ uav.GetCpuHandle() },
            resource.GetD3D12Resource(),
            (const float*)&color,
            0,
            nullptr // Clears entire texture
        );
    }

    void ComputeCmdList::Dispatch(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadGroupSize)
    {
        BenzinAssert(dimension.x != 0 && dimension.y != 0 && dimension.z != 0);
        BenzinAssert(threadGroupSize.x != 0 && threadGroupSize.y != 0 && threadGroupSize.z != 0);

        const DirectX::XMUINT3 threadGroupCount
        {
            std::max(DivideUp(dimension.x, threadGroupSize.x), 1u),
            std::max(DivideUp(dimension.y, threadGroupSize.y), 1u),
            std::max(DivideUp(dimension.z, threadGroupSize.z), 1u),
        };

        m_D3D12GraphicsCommandList1->Dispatch(threadGroupCount.x, threadGroupCount.y, threadGroupCount.z);
    }

    void ComputeCmdList::BuildRayTracingAccelerationStructure(const RayTracing_AcclerationStructure& accelerationStructure)
    {
        BenzinAssert(m_D3D12GraphicsCommandList4 != nullptr);
        BenzinAssert(accelerationStructure.GetScratchResource()->GetCurrentState() == ResourceState::UnorderedAccess);

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC d3d12BuildAccelerationStructureDesc
        {
            .DestAccelerationStructureData = accelerationStructure.GetBuffer()->GetGpuVirtualAddress(),
            .Inputs = accelerationStructure.GetD3D12BuildInputs(),
            .SourceAccelerationStructureData = 0,
            .ScratchAccelerationStructureData = accelerationStructure.GetScratchResource()->GetGpuVirtualAddress(),
        };

        m_D3D12GraphicsCommandList4->BuildRaytracingAccelerationStructure(&d3d12BuildAccelerationStructureDesc, 0, nullptr);
    }

    void ComputeCmdList::SetRayTracingPso(const RayTracing_Pso& pso)
    {
        BenzinAssert(m_D3D12GraphicsCommandList4 != nullptr);
        BenzinAssert(pso.GetD3D12StateObject() != nullptr);

        m_D3D12GraphicsCommandList4->SetPipelineState1(pso.GetD3D12StateObject());
    }

    void ComputeCmdList::DispatchRays(const RayTracing_ShaderTable& shaderTable, const DirectX::XMUINT3 dimenions)
    {
        BenzinAssert(m_D3D12GraphicsCommandList4 != nullptr);
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

        m_D3D12GraphicsCommandList4->DispatchRays(&d3d12DispatchRayDesc);
    }

    // GraphicsCmdList

    GraphicsCmdList::GraphicsCmdList(Device& device)
        : ComputeCmdList{ device }  
    {
        BenzinD3D12Call(m_D3D12GraphicsCommandList1->QueryInterface(IID_PPV_ARGS(&m_D3D12GraphicsCommandList6)));
    }

    GraphicsCmdList::~GraphicsCmdList()
    {
        SafeReleaseD3DObject(m_D3D12GraphicsCommandList6, false);
    }

    void GraphicsCmdList::SetGraphicsCbv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList1->SetGraphicsRootConstantBufferView(+rootParameter, gpuVirtualAddress);
    }

    void GraphicsCmdList::SetGraphicsSrv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList1->SetGraphicsRootShaderResourceView(+rootParameter, gpuVirtualAddress);
    }

    void GraphicsCmdList::SetGraphicsRootConstant(uint32_t rootIndex, uint32_t value)
    {
        m_D3D12GraphicsCommandList1->SetGraphicsRoot32BitConstant(+UnifiedRootParameter::Root32Consts, value, rootIndex);
    }

    void GraphicsCmdList::SetGraphicsRootResource(uint32_t rootIndex, const Descriptor& viewDescriptor)
    {
        BenzinAssert(viewDescriptor.IsGpuValid());
        SetGraphicsRootConstant(rootIndex, viewDescriptor.GetGpuHeapIndex());
    }

    void GraphicsCmdList::SetVertexPso(const VertexPso& pso)
    {
        BenzinAssert(pso.GetD3D12PipelineState() != nullptr);
        m_D3D12GraphicsCommandList1->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void GraphicsCmdList::SetVertexBuffer(const Buffer& vertexBuffer)
    {
        BenzinAssert(vertexBuffer.GetType() == BufferType::Vertex);

        const D3D12_VERTEX_BUFFER_VIEW d3d12VertexBufferView
        {
            .BufferLocation = vertexBuffer.GetGpuVirtualAddress(),
            .SizeInBytes = vertexBuffer.GetSize(),
            .StrideInBytes = vertexBuffer.GetElementSize(),
        };

        m_D3D12GraphicsCommandList1->IASetVertexBuffers(0, 1, &d3d12VertexBufferView);
    }

    void GraphicsCmdList::SetIndexBuffer(const Buffer& indexBuffer)
    {
        BenzinAssert(indexBuffer.GetType() == BufferType::Index);
        BenzinAssert(indexBuffer.GetFormat() == GraphicsFormat::R16Uint || indexBuffer.GetFormat() == GraphicsFormat::R32Uint);

        const D3D12_INDEX_BUFFER_VIEW d3d12VertexBufferView
        {
            .BufferLocation = indexBuffer.GetGpuVirtualAddress(),
            .SizeInBytes = indexBuffer.GetSize(),
            .Format = (DXGI_FORMAT)indexBuffer.GetFormat(),
        };

        m_D3D12GraphicsCommandList1->IASetIndexBuffer(&d3d12VertexBufferView);
    }

    void GraphicsCmdList::SetPrimitiveTopology(PrimitiveTopology primitiveTopology)
    {
        BenzinAssert(primitiveTopology != PrimitiveTopology::Unknown);

        m_D3D12GraphicsCommandList1->IASetPrimitiveTopology((D3D12_PRIMITIVE_TOPOLOGY)primitiveTopology);
    }

    void GraphicsCmdList::SetViewport(const Viewport& viewport)
    {
        m_D3D12GraphicsCommandList1->RSSetViewports(1, reinterpret_cast<const D3D12_VIEWPORT*>(&viewport));
    }

    void GraphicsCmdList::SetScissorRect(const ScissorRect& scissorRect)
    {
        const D3D12_RECT d3d12Rect
        {
            .left = (LONG)scissorRect.X,
            .top = (LONG)scissorRect.Y,
            .right = (LONG)(scissorRect.X + scissorRect.Width),
            .bottom = (LONG)(scissorRect.Y + scissorRect.Height),
        };

        m_D3D12GraphicsCommandList1->RSSetScissorRects(1, &d3d12Rect);
    }

    void GraphicsCmdList::SetBlendFactor(const DirectX::XMFLOAT4& color)
    {
        m_D3D12GraphicsCommandList1->OMSetBlendFactor((const float*)&color);
    }

    void GraphicsCmdList::SetRenderTargets(const std::vector<Descriptor>& rtvs, const Descriptor* dsv)
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

            m_D3D12GraphicsCommandList1->OMSetRenderTargets(
                (uint32_t)d3d12RtvDescriptorHandles.size(),
                d3d12RtvDescriptorHandles.data(),
                isRenderTargetContiguous,
                &d3d12DsvDescriptorHandle
            );
        }
        else
        {
            m_D3D12GraphicsCommandList1->OMSetRenderTargets(
                (uint32_t)d3d12RtvDescriptorHandles.size(),
                d3d12RtvDescriptorHandles.data(),
                isRenderTargetContiguous,
                nullptr
            );
        }
    }

    void GraphicsCmdList::ClearRenderTarget(const Texture& renderTarget, std::optional<DirectX::XMFLOAT4> overrideClearColor)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12RtvDescriptorHandle{ renderTarget.GetRtv().GetCpuHandle() };
        const DirectX::XMFLOAT4& clearValue = overrideClearColor.value_or(MakeLazyConverter([&renderTarget] { return renderTarget.GetClearColor(); }));

        m_D3D12GraphicsCommandList1->ClearRenderTargetView(d3d12RtvDescriptorHandle, reinterpret_cast<const float*>(&clearValue), 0, nullptr);
    }

    void GraphicsCmdList::ClearDepthStencil(const Texture& depthStencil)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE d3d12DsvDescriptorHandle{ depthStencil.GetDsv().GetCpuHandle() };
        const auto clearDepthStencil = depthStencil.GetClearDepthStencil();

        m_D3D12GraphicsCommandList1->ClearDepthStencilView(
            d3d12DsvDescriptorHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            clearDepthStencil.Depth,
            clearDepthStencil.Stencil,
            0,
            nullptr
        );
    }

    void GraphicsCmdList::DrawVertexed(uint32_t vertexCount, uint32_t instanceCount)
    {
        constexpr uint32_t startVertexLocation = 0;

        m_D3D12GraphicsCommandList1->DrawInstanced(vertexCount, instanceCount, startVertexLocation, 0);
    }

    void GraphicsCmdList::DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount)
    {
        m_D3D12GraphicsCommandList1->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, 0);
    }

    void GraphicsCmdList::SetMeshPso(const MeshPso& pso)
    {
        BenzinAssert(m_D3D12GraphicsCommandList6 != nullptr);
        BenzinAssert(pso.GetD3D12PipelineState() != nullptr);

        m_D3D12GraphicsCommandList6->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void GraphicsCmdList::DispatchMesh(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadGroupSize)
    {
        BenzinAssert(m_D3D12GraphicsCommandList6 != nullptr);
        BenzinAssert(dimension.x != 0 && dimension.y != 0 && dimension.z != 0);
        BenzinAssert(threadGroupSize.x != 0 && threadGroupSize.y != 0 && threadGroupSize.z != 0);

        const DirectX::XMUINT3 threadGroupCount
        {
            std::max(DivideUp(dimension.x, threadGroupSize.x), 1u),
            std::max(DivideUp(dimension.y, threadGroupSize.y), 1u),
            std::max(DivideUp(dimension.z, threadGroupSize.z), 1u),
        };

        m_D3D12GraphicsCommandList6->DispatchMesh(threadGroupCount.x, threadGroupCount.y, threadGroupCount.z);
    }

    // ScopedResourceBarriers

    ScopedResourceBarriers::ScopedResourceBarriers(CmdList& cmdList, std::span<const ResourceBarrierVariant> resourceBarriers)
        : m_CmdList{ cmdList }
    {
        for (const auto& resourceBarrier : resourceBarriers)
        {
            const auto* transitionBarrier = std::get_if<TransitionBarrier>(&resourceBarrier);
            if (transitionBarrier != nullptr)
            {
                auto& swappedBarrier = m_SwappedTransitionBarriers.emplace_back(*transitionBarrier);
                std::swap(swappedBarrier.StateAfter, swappedBarrier.StateBefore);
            }

            m_CmdList.AddResourceBarrier(resourceBarrier);
        }

        m_CmdList.FlushResourceBarriers();
    }

    ScopedResourceBarriers::~ScopedResourceBarriers()
    {
        BenzinAssert(!m_SwappedTransitionBarriers.empty());

        for (auto& transitionBarrier : m_SwappedTransitionBarriers)
        {
            m_CmdList.AddResourceBarrier(transitionBarrier);
        }

        m_CmdList.FlushResourceBarriers();
    }

    // ScopedGpuEvent

    ScopedGpuEvent::ScopedGpuEvent(CmdList& cmdList, std::string_view name)
        : m_D3D12GraphicsCommandList{ cmdList.GetD3D12GraphicsCommandList() }
    {
        PIXBeginEvent(m_D3D12GraphicsCommandList, PIX_COLOR_DEFAULT, "%s", name.data());
    }

    ScopedGpuEvent::~ScopedGpuEvent()
    {
        PIXEndEvent(m_D3D12GraphicsCommandList);
    }

}
