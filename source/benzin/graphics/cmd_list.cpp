#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/cmd_list.hpp>

#include <benzin/core/buffer_writer.hpp>
#include <benzin/core/math.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/descriptor_manager.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/pso.hpp>
#include <benzin/graphics/query_heap.hpp>
#include <benzin/graphics/ray_tracing_acceleration_structures.hpp>
#include <benzin/graphics/ray_tracing_pso.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

// Ref: https://devblogs.microsoft.com/pix/winpixeventruntime/
#define USE_PIX
#include <pix3.h>

namespace benzin
{

    // CmdList

    CmdList::CmdList(Device& device)
    {
        BenzinD3D12Call(device.GetD3D12Device()->CreateCommandList1(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&m_D3D12GraphicsCommandList1)));
        BenzinEnsure(m_D3D12GraphicsCommandList1 != nullptr);

        SetD3DObjectDebugName(m_D3D12GraphicsCommandList1, "GraphicsCmdList");
    }

    CmdList::~CmdList()
    {
        SafeReleaseD3DObject(m_D3D12GraphicsCommandList1);
    }

    void CmdList::AddTransition(const Resource& resource, D3D12_RESOURCE_STATES d3d12StateAfter)
    {
        m_DeferredTransitionBarriers.emplace_back(&resource, d3d12StateAfter);
    }

    void CmdList::AddUnorderedAccess(const Resource& resource)
    {
        BenzinAssert(resource.GetD3D12State() == D3D12_RESOURCE_STATE_UNORDERED_ACCESS || resource.GetD3D12State() == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

        m_DeferredUnorderedAccessBarriers.emplace_back(&resource);
    }

    void CmdList::FlushBarriers()
    {
        std::vector<D3D12_RESOURCE_BARRIER> d3d12Barriers;
        d3d12Barriers.reserve(m_DeferredTransitionBarriers.size() + m_DeferredUnorderedAccessBarriers.size());

        for (const TransitionBarrier& barrier : m_DeferredTransitionBarriers)
        {
            if ((barrier.m_Resource->GetD3D12State() & barrier.m_D3D12StateAfter) != 0)
                continue;

            D3D12_RESOURCE_BARRIER d3d12Barrier = {};
            d3d12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            d3d12Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            d3d12Barrier.Transition.pResource = barrier.m_Resource->GetD3D12Resource();
            d3d12Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            d3d12Barrier.Transition.StateBefore = barrier.m_Resource->GetD3D12State();
            d3d12Barrier.Transition.StateAfter = barrier.m_D3D12StateAfter;

            d3d12Barriers.push_back(d3d12Barrier);
            barrier.m_Resource->SetD3D12State(barrier.m_D3D12StateAfter);
        }

        for (const Resource* resource : m_DeferredUnorderedAccessBarriers)
        {
            D3D12_RESOURCE_BARRIER d3d12Barrier = {};
            d3d12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            d3d12Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            d3d12Barrier.UAV.pResource = resource->GetD3D12Resource();

            d3d12Barriers.push_back(d3d12Barrier);
        }

        if (!d3d12Barriers.empty())
        {
            m_D3D12GraphicsCommandList1->ResourceBarrier((uint32_t)d3d12Barriers.size(), d3d12Barriers.data());
        }

        m_DeferredTransitionBarriers.clear();
        m_DeferredUnorderedAccessBarriers.clear();
    }

    // CopyCmdList

    void CopyCmdList::CopyResource(const Resource& destResource, const Resource& sourceResource)
    {
        m_D3D12GraphicsCommandList1->CopyResource(destResource.GetD3D12Resource(), sourceResource.GetD3D12Resource());
    }

    void CopyCmdList::CopyBufferRegion(const Buffer& destBuffer, uint64_t destOffsetInBytes, const Buffer& sourceBuffer, uint64_t sourceOffsetInBytes, uint64_t dataSizeInBytes)
    {
        BenzinAssert(dataSizeInBytes != 0);

        BenzinAssert(destOffsetInBytes + dataSizeInBytes <= destBuffer.GetSizeInBytes());
        BenzinAssert(sourceOffsetInBytes + dataSizeInBytes <= sourceBuffer.GetSizeInBytes());

        AddTransition(destBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
        AddTransition(sourceBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
        FlushBarriers();

        m_D3D12GraphicsCommandList1->CopyBufferRegion(
            destBuffer.GetD3D12Resource(),
            destOffsetInBytes,
            sourceBuffer.GetD3D12Resource(),
            sourceOffsetInBytes,
            dataSizeInBytes);
    }

    void CopyCmdList::CopyTextureRegion(const Texture& destTexture, uint32_t destSubResourceIndex, const Texture& sourceTexture, uint32_t sourceSubresourceIndex)
    {
        BenzinAssert(destTexture.GetDxgiFormat() == sourceTexture.GetDxgiFormat());

        D3D12_TEXTURE_COPY_LOCATION d3d12DestLocatiton = {};
        d3d12DestLocatiton.pResource = destTexture.GetD3D12Resource();
        d3d12DestLocatiton.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        d3d12DestLocatiton.SubresourceIndex = destSubResourceIndex;

        D3D12_TEXTURE_COPY_LOCATION d3d12SourceLocatiton = {};
        d3d12SourceLocatiton.pResource = sourceTexture.GetD3D12Resource();
        d3d12SourceLocatiton.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        d3d12SourceLocatiton.SubresourceIndex = sourceSubresourceIndex;

        AddTransition(destTexture, D3D12_RESOURCE_STATE_COPY_DEST);
        AddTransition(sourceTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
        FlushBarriers();

        m_D3D12GraphicsCommandList1->CopyTextureRegion(&d3d12DestLocatiton, 0, 0, 0, &d3d12SourceLocatiton, nullptr);
    }

    void CopyCmdList::UploadToBuffer(Buffer& destBuffer, std::span<const std::byte> data, uint64_t destOffsetInBytes)
    {
        BenzinAssert(m_UploadBuffer != nullptr);
        BenzinAssert(!data.empty());

        const uint64_t uploadOffsetInBytes = AllocateInUploadBuffer(data.size_bytes());

        BufferWriter writer = MakeBufferWriter(*m_UploadBuffer);
        writer.SetPositionInBytes(uploadOffsetInBytes);
        writer.WriteData(data);

        CopyBufferRegion(destBuffer, destOffsetInBytes, *m_UploadBuffer, uploadOffsetInBytes, data.size_bytes());
    }

    void CopyCmdList::UploadToTexture(Texture& texture, std::span<const SubResourceData> subResources)
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
            const uint64_t offsetInBytes = AllocateInUploadBuffer(0, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

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

            AllocateInUploadBuffer(resourceSizeInBytes, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        }

        // Copying sub-resources to UploadBuffer
        // Go down to rows and copy it
        {
            BufferWriter writer = MakeBufferWriter(*m_UploadBuffer);

            for (uint32_t subResourceIndex = 0; subResourceIndex < (uint32_t)subResources.size(); ++subResourceIndex)
            {
                const SubResourceData& subResource = subResources[subResourceIndex];
                const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& d3d12Layout = copyableFootprits.D3D12Layouts[subResourceIndex];

                // SubResource data
                const uint64_t destOffsetInBytes = d3d12Layout.Offset;
                const std::byte* sourceData = subResource.m_Data;

                for (uint32_t sliceIndex = 0; sliceIndex < d3d12Layout.Footprint.Depth; ++sliceIndex)
                {
                    const uint64_t rowCount = copyableFootprits.RowCounts[subResourceIndex];
                    const uint64_t destSlicePitchInBytes = d3d12Layout.Footprint.RowPitch * rowCount;

                    // Slice data
                    const uint64_t destSliceOffsetInBytes = destOffsetInBytes + destSlicePitchInBytes * sliceIndex;
                    const std::byte* sourceSliceData = sourceData + subResource.m_SlicePitchInBytes * sliceIndex;

                    for (uint32_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                    {
                        const uint64_t destRowPitchInBytes = d3d12Layout.Footprint.RowPitch;

                        // Row data
                        const uint64_t destRowOffsetInBytes = destSliceOffsetInBytes + destRowPitchInBytes * rowIndex;
                        const std::byte* sourceRowData = sourceSliceData + subResource.m_RowPitchInBytes * rowIndex;

                        const uint64_t rowSizeInBytes = copyableFootprits.RowSizesInBytes[subResourceIndex];

                        writer.SetPositionInBytes(destRowOffsetInBytes);
                        writer.WriteData(ToSpan(sourceRowData, rowSizeInBytes));
                    }
                }
            }
        }

        AddTransition(texture, D3D12_RESOURCE_STATE_COPY_DEST);
        AddTransition(*m_UploadBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
        FlushBarriers();

        // Copy to texture
        for (uint32_t i = 0; i < subResources.size(); ++i)
        {
            D3D12_TEXTURE_COPY_LOCATION d3d12DestLocation = {};
            d3d12DestLocation.pResource = texture.GetD3D12Resource();
            d3d12DestLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            d3d12DestLocation.SubresourceIndex = i;

            D3D12_TEXTURE_COPY_LOCATION d3d12SourceLocation = {};
            d3d12SourceLocation.pResource = m_UploadBuffer->GetD3D12Resource();
            d3d12SourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            d3d12SourceLocation.PlacedFootprint = copyableFootprits.D3D12Layouts[i];

            m_D3D12GraphicsCommandList1->CopyTextureRegion(&d3d12DestLocation, 0, 0, 0, &d3d12SourceLocation, nullptr);
        }
    }

    void CopyCmdList::UploadToTexture(Texture& texture, std::span<const std::byte> data)
    {
        BenzinAssert(texture.GetMipCount() == 1);

        const uint32_t pixelSizeInBytes = GetDxgiFormatSizeInBytes(texture.GetDxgiFormat());
        const uint64_t rowPitchInBytes = pixelSizeInBytes * texture.GetWidth();
        const uint64_t slicePitchInBytes = rowPitchInBytes * texture.GetHeight();

        BenzinAssert(slicePitchInBytes * texture.GetDepth() == data.size_bytes());

        std::vector<SubResourceData> subResources;
        subResources.reserve(texture.GetDepth());

        for (uint16_t depthIndex = 0; depthIndex < texture.GetDepth(); ++depthIndex)
        {
            SubResourceData subResourceData;
            subResourceData.m_Data = data.data() + slicePitchInBytes * depthIndex;
            subResourceData.m_RowPitchInBytes = rowPitchInBytes;
            subResourceData.m_SlicePitchInBytes = slicePitchInBytes;

            subResources.push_back(subResourceData);
        }

        UploadToTexture(texture, subResources);
    }

    void CopyCmdList::SetUploadBuffer(Buffer& uploadBuffer)
    {
        m_UploadBuffer = &uploadBuffer;
        m_UploadBufferOffsetInBytes = 0;
    }

    uint64_t CopyCmdList::AllocateInUploadBuffer(uint64_t sizeInBytes, uint32_t alignmentInBytes)
    {
        BenzinEnsure(m_UploadBuffer != nullptr);

        uint64_t alignedOffsetInBytes = m_UploadBufferOffsetInBytes;
        if (alignmentInBytes != 0)
        {
            alignedOffsetInBytes = AlignUp(m_UploadBufferOffsetInBytes, alignmentInBytes);
        }

        m_UploadBufferOffsetInBytes = alignedOffsetInBytes + sizeInBytes;
        BenzinEnsure(m_UploadBufferOffsetInBytes <= m_UploadBuffer->GetSizeInBytes());

        return alignedOffsetInBytes;
    }

    // ComputeCmdList

    ComputeCmdList::ComputeCmdList(Device& device)
        : CopyCmdList{ device }
    {
        BenzinD3D12Call(m_D3D12GraphicsCommandList1->QueryInterface(IID_PPV_ARGS(&m_D3D12GraphicsCommandList4)));
        BenzinEnsure(m_D3D12GraphicsCommandList4 != nullptr);
    }

    ComputeCmdList::~ComputeCmdList()
    {
        SafeReleaseD3DObject(m_D3D12GraphicsCommandList4, false);
    }

    void ComputeCmdList::SetTimestamp(const QueryHeap& timestampQueryHeap, uint32_t index)
    {
        BenzinAssert(index < timestampQueryHeap.GetCount());

        m_D3D12GraphicsCommandList1->EndQuery(timestampQueryHeap.GetD3D12QueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, index);
    }

    void ComputeCmdList::ResolveTimestamps(const QueryHeap& timestampQueryHeap, const Buffer& readbackBuffer, uint64_t readbackOffsetInBytes)
    {
        BenzinAssert(readbackBuffer.GetHeapType() == GpuHeapType::Readback);

        m_D3D12GraphicsCommandList1->ResolveQueryData(
            timestampQueryHeap.GetD3D12QueryHeap(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            0,
            timestampQueryHeap.GetCount(),
            readbackBuffer.GetD3D12Resource(),
            readbackOffsetInBytes);
    }

    void ComputeCmdList::SetComputeCbv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList1->SetComputeRootConstantBufferView(*rootParameter, gpuVirtualAddress);
    }

    void ComputeCmdList::SetComputeSrv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList1->SetComputeRootShaderResourceView(*rootParameter, gpuVirtualAddress);
    }

    void ComputeCmdList::SetComputeUav(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList1->SetComputeRootUnorderedAccessView(*rootParameter, gpuVirtualAddress);
    }

    void ComputeCmdList::SetComputeRootConstant(uint32_t rootIndex, uint32_t value)
    {
        m_D3D12GraphicsCommandList1->SetComputeRoot32BitConstant(*UnifiedRootParameter::Root32Consts, value, rootIndex);
    }

    void ComputeCmdList::SetComputeRootSrv(uint32_t rootIndex, const Buffer& buffer)
    {
        AddTransition(buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        SetComputeRootConstant(rootIndex, buffer.GetSrv().GetGpuHeapIndex());
    }

    void ComputeCmdList::SetComputeRootSrv(uint32_t rootIndex, const Texture& texture, const TextureSrv& srv)
    {
        AddTransition(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        SetComputeRootConstant(rootIndex, texture.GetSrv(srv).GetGpuHeapIndex());
    }

    void ComputeCmdList::SetComputeRootUav(uint32_t rootIndex, const Buffer& buffer)
    {
        AddTransition(buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        SetComputeRootConstant(rootIndex, buffer.GetUav().GetGpuHeapIndex());
    }

    void ComputeCmdList::SetComputeRootUav(uint32_t rootIndex, const Texture& texture)
    {
        AddTransition(texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        SetComputeRootConstant(rootIndex, texture.GetUav().GetGpuHeapIndex());
    }

    void ComputeCmdList::SetComputePso(const ComputePso& pso)
    {
        m_D3D12GraphicsCommandList1->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void ComputeCmdList::ClearUnorderedAccess(const Resource& resource, const Descriptor& uav, const DirectX::XMFLOAT4& color)
    {
        BenzinAssert(resource.GetD3D12State() == D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        BenzinAssert(uav.IsCpuValid());
        BenzinAssert(uav.IsGpuValid());

        m_D3D12GraphicsCommandList1->ClearUnorderedAccessViewFloat(
            D3D12_GPU_DESCRIPTOR_HANDLE{ uav.GetGpuHandle() },
            D3D12_CPU_DESCRIPTOR_HANDLE{ uav.GetCpuHandle() },
            resource.GetD3D12Resource(),
            (const float*)&color,
            0,
            nullptr /* Clears entire texture */);
    }

    void ComputeCmdList::Dispatch(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadGroupSize)
    {
        BenzinAssert(dimension.x * dimension.y * dimension.z != 0);
        BenzinAssert(threadGroupSize.x * threadGroupSize.y * threadGroupSize.z != 0);

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
        BenzinAssert(accelerationStructure.GetScratchResource()->GetD3D12State() == D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC d3d12BuildDesc = {};
        d3d12BuildDesc.DestAccelerationStructureData = accelerationStructure.GetBuffer()->GetGpuVirtualAddress();
        d3d12BuildDesc.Inputs = accelerationStructure.GetD3D12BuildInputs();
        d3d12BuildDesc.SourceAccelerationStructureData = 0;
        d3d12BuildDesc.ScratchAccelerationStructureData = accelerationStructure.GetScratchResource()->GetGpuVirtualAddress();

        m_D3D12GraphicsCommandList4->BuildRaytracingAccelerationStructure(&d3d12BuildDesc, 0, nullptr);
    }

    void ComputeCmdList::SetRayTracingPso(const RayTracing_Pso& pso)
    {
        m_D3D12GraphicsCommandList4->SetPipelineState1(pso.GetD3D12StateObject());
    }

    void ComputeCmdList::DispatchRays(const RayTracing_ShaderTable& shaderTable, const DirectX::XMUINT3 dimenions)
    {
        BenzinAssert(dimenions.x * dimenions.y * dimenions.z != 0);

        const RayTracing_ShaderTable::GpuAddresses& gpuAddresses = shaderTable.GetGpuAddresses();

        D3D12_DISPATCH_RAYS_DESC d3d12DispatchRayDesc = {};
        d3d12DispatchRayDesc.RayGenerationShaderRecord.StartAddress = gpuAddresses.m_RayGenerationShader.m_GpuVirtualAddress;
        d3d12DispatchRayDesc.RayGenerationShaderRecord.SizeInBytes = gpuAddresses.m_RayGenerationShader.m_SizeInBytes;
        d3d12DispatchRayDesc.MissShaderTable.StartAddress = gpuAddresses.m_MissTable.m_GpuVirtualAddress;
        d3d12DispatchRayDesc.MissShaderTable.SizeInBytes = gpuAddresses.m_MissTable.m_SizeInBytes;
        d3d12DispatchRayDesc.MissShaderTable.StrideInBytes = 0; // TODO: For now supported only one record per table
        d3d12DispatchRayDesc.HitGroupTable.StartAddress = gpuAddresses.m_HitGroupTable.m_GpuVirtualAddress,
        d3d12DispatchRayDesc.HitGroupTable.SizeInBytes = gpuAddresses.m_MissTable.m_SizeInBytes,
        d3d12DispatchRayDesc.HitGroupTable.StrideInBytes = 0; // TODO: For now supported only one record per table
        d3d12DispatchRayDesc.CallableShaderTable.StartAddress = 0;
        d3d12DispatchRayDesc.CallableShaderTable.SizeInBytes = 0;
        d3d12DispatchRayDesc.CallableShaderTable.StrideInBytes = 0;
        d3d12DispatchRayDesc.Width = dimenions.x;
        d3d12DispatchRayDesc.Height = dimenions.y;
        d3d12DispatchRayDesc.Depth = dimenions.z;

        m_D3D12GraphicsCommandList4->DispatchRays(&d3d12DispatchRayDesc);
    }

    // GraphicsCmdList

    GraphicsCmdList::GraphicsCmdList(Device& device)
        : ComputeCmdList{ device }  
    {
        BenzinD3D12Call(m_D3D12GraphicsCommandList1->QueryInterface(IID_PPV_ARGS(&m_D3D12GraphicsCommandList6)));
        BenzinEnsure(m_D3D12GraphicsCommandList6 != nullptr);
    }

    GraphicsCmdList::~GraphicsCmdList()
    {
        SafeReleaseD3DObject(m_D3D12GraphicsCommandList6, false);
    }

    void GraphicsCmdList::SetGraphicsCbv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList1->SetGraphicsRootConstantBufferView(*rootParameter, gpuVirtualAddress);
    }

    void GraphicsCmdList::SetGraphicsSrv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList1->SetGraphicsRootShaderResourceView(*rootParameter, gpuVirtualAddress);
    }

    void GraphicsCmdList::SetGraphicsUav(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress)
    {
        m_D3D12GraphicsCommandList1->SetGraphicsRootUnorderedAccessView(*rootParameter, gpuVirtualAddress);
    }

    void GraphicsCmdList::SetGraphicsRootConstant(uint32_t rootIndex, uint32_t value)
    {
        m_D3D12GraphicsCommandList1->SetGraphicsRoot32BitConstant(*UnifiedRootParameter::Root32Consts, value, rootIndex);
    }

    void GraphicsCmdList::SetGraphicsRootSrv(uint32_t rootIndex, const Buffer& buffer, D3D12_RESOURCE_STATES d3d12State)
    {
        AddTransition(buffer, d3d12State);
        SetGraphicsRootConstant(rootIndex, buffer.GetSrv().GetGpuHeapIndex());
    }

    void GraphicsCmdList::SetGraphicsRootSrv(uint32_t rootIndex, const Texture& texture, D3D12_RESOURCE_STATES d3d12State)
    {
        AddTransition(texture, d3d12State);
        SetGraphicsRootConstant(rootIndex, texture.GetSrv().GetGpuHeapIndex());
    }

    void GraphicsCmdList::SetVertexPso(const VertexPso& pso)
    {
        m_D3D12GraphicsCommandList1->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void GraphicsCmdList::SetVertexBuffer(const Buffer& vertexBuffer)
    {
        BenzinAssert(vertexBuffer.GetType() == BufferType::Structured);

        AddTransition(vertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        D3D12_VERTEX_BUFFER_VIEW d3d12View = {};
        d3d12View.BufferLocation = vertexBuffer.GetGpuVirtualAddress();
        d3d12View.SizeInBytes = (uint32_t)vertexBuffer.GetSizeInBytes();
        d3d12View.StrideInBytes = vertexBuffer.GetElementSizeInBytes();

        m_D3D12GraphicsCommandList1->IASetVertexBuffers(0, 1, &d3d12View);
    }

    void GraphicsCmdList::SetIndexBuffer(const Buffer& indexBuffer)
    {
        BenzinAssert(indexBuffer.GetType() == BufferType::Format);
        BenzinAssert(indexBuffer.GetDxgiFormat() == DXGI_FORMAT_R16_UINT || indexBuffer.GetDxgiFormat() == DXGI_FORMAT_R32_UINT);

        AddTransition(indexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);

        D3D12_INDEX_BUFFER_VIEW d3d12View = {};
        d3d12View.BufferLocation = indexBuffer.GetGpuVirtualAddress();
        d3d12View.SizeInBytes = (uint32_t)indexBuffer.GetSizeInBytes();
        d3d12View.Format = indexBuffer.GetDxgiFormat();

        m_D3D12GraphicsCommandList1->IASetIndexBuffer(&d3d12View);
    }

    void GraphicsCmdList::AddRenderTarget(const Texture& texture)
    {
        BenzinAssert(m_DeferredD3D12Rtvs.size() < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

        AddTransition(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_DeferredD3D12Rtvs.emplace_back(texture.GetRtv().GetCpuHandle());
    }

    void GraphicsCmdList::AddDepthStencil(const Texture& texture, D3D12_RESOURCE_STATES d3d12State)
    {
        AddTransition(texture, d3d12State);
        m_DeferredD3D12Dsv.ptr = texture.GetDsv().GetCpuHandle();
    }

    void GraphicsCmdList::SetRenderTargets()
    {
        BenzinAssert(!m_DeferredD3D12Rtvs.empty());

        m_D3D12GraphicsCommandList1->OMSetRenderTargets(
            (uint32_t)m_DeferredD3D12Rtvs.size(),
            m_DeferredD3D12Rtvs.data(),
            false,
            m_DeferredD3D12Dsv.ptr != 0 ? &m_DeferredD3D12Dsv : nullptr);

        m_DeferredD3D12Rtvs.clear();
        m_DeferredD3D12Dsv = {};
    }

    void GraphicsCmdList::ClearRenderTarget(const Texture& texture, std::optional<DirectX::XMFLOAT4> clearColor)
    {
        BenzinAssert(texture.GetD3D12State() == D3D12_RESOURCE_STATE_RENDER_TARGET);

        D3D12_CPU_DESCRIPTOR_HANDLE d3d12Rtv = {};
        d3d12Rtv.ptr = texture.GetRtv().GetCpuHandle();

        const DirectX::XMFLOAT4& clearValue = clearColor.value_or(MakeLazyConverter([&texture] { return texture.GetClearColor(); }));

        m_D3D12GraphicsCommandList1->ClearRenderTargetView(d3d12Rtv, (const float*)&clearValue, 0, nullptr);
    }

    void GraphicsCmdList::ClearDepthStencil(const Texture& texture)
    {
        BenzinAssert(texture.GetD3D12State() == D3D12_RESOURCE_STATE_DEPTH_WRITE);

        D3D12_CPU_DESCRIPTOR_HANDLE d3d12Dsv = {};
        d3d12Dsv.ptr = texture.GetDsv().GetCpuHandle();

        m_D3D12GraphicsCommandList1->ClearDepthStencilView(
            d3d12Dsv,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            texture.GetClearDepthStencil().m_Depth,
            texture.GetClearDepthStencil().m_Stencil,
            0,
            nullptr);
    }

    void GraphicsCmdList::DrawVertexed(uint32_t vertexCount, uint32_t instanceCount)
    {
        m_D3D12GraphicsCommandList1->DrawInstanced(vertexCount, instanceCount, 0, 0);
    }

    void GraphicsCmdList::DrawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset, uint32_t instanceCount)
    {
        m_D3D12GraphicsCommandList1->DrawIndexedInstanced(indexCount, instanceCount, indexOffset, vertexOffset, 0);
    }

    void GraphicsCmdList::SetMeshPso(const MeshPso& pso)
    {
        m_D3D12GraphicsCommandList6->SetPipelineState(pso.GetD3D12PipelineState());
    }

    void GraphicsCmdList::DispatchMesh(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadGroupSize)
    {
        BenzinAssert(dimension.x * dimension.y * dimension.z != 0);
        BenzinAssert(threadGroupSize.x * threadGroupSize.y * threadGroupSize.z != 0);

        const DirectX::XMUINT3 threadGroupCount
        {
            std::max(DivideUp(dimension.x, threadGroupSize.x), 1u),
            std::max(DivideUp(dimension.y, threadGroupSize.y), 1u),
            std::max(DivideUp(dimension.z, threadGroupSize.z), 1u),
        };

        BenzinAssert(
            threadGroupCount.x <= std::numeric_limits<uint16_t>::max() &&
            threadGroupCount.y <= std::numeric_limits<uint16_t>::max() &&
            threadGroupCount.z <= std::numeric_limits<uint16_t>::max());

        m_D3D12GraphicsCommandList6->DispatchMesh(threadGroupCount.x, threadGroupCount.y, threadGroupCount.z);
    }

    // ScopedGpuEvent

    ScopedGpuEvent::ScopedGpuEvent(std::string_view name)
    {
        auto* d3d12CommandList = ms_Device->GetGraphicsCmdQueue().GetCmdList().GetD3D12GraphicsCommandList();
        PIXBeginEvent(d3d12CommandList, PIX_COLOR_DEFAULT, "%s", name.data());
    }

    ScopedGpuEvent::~ScopedGpuEvent()
    {
        auto* d3d12CommandList = ms_Device->GetGraphicsCmdQueue().GetCmdList().GetD3D12GraphicsCommandList();
        PIXEndEvent(d3d12CommandList);
    }

    void ScopedGpuEvent::SetContext(Device& device)
    {
        ms_Device = &device;
    }

}
