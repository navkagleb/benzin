#pragma once

#include <benzin/graphics/common.hpp>
#include <benzin/graphics/resource.hpp>

namespace benzin
{

    class Buffer;
    class ComputePso;
    class Descriptor;
    class MeshPso;
    class QueryHeap;
    class RayTracing_AcclerationStructure;
    class RayTracing_Pso;
    class RayTracing_ShaderTable;
    class Texture;
    class VertexPso;

    struct SubResourceData;

    enum class UnifiedRootParameter;

    struct TransitionBarrier
    {
        const Resource& TransitionResource;
        ResourceState StateBefore;
        ResourceState StateAfter;

        TransitionBarrier(const Resource& resource, ResourceState stateAfter)
            : TransitionResource{ resource }
            , StateBefore{ resource.GetCurrentState() }
            , StateAfter{ stateAfter }
        {}
    };

    struct UnorderedAccessBarrier
    {
        const Resource& Resource;
    };

    using ResourceBarrierVariant = std::variant<TransitionBarrier, UnorderedAccessBarrier>;

    class CmdList
    {
    public:
        explicit CmdList(Device& device);
        virtual ~CmdList();

        auto* GetD3D12GraphicsCommandList() const { return m_D3D12GraphicsCommandList1; }

        void AddResourceBarrier(const ResourceBarrierVariant& resourceBarrierVariant, bool isNeedToFlush = false);
        void FlushResourceBarriers();

    protected:
        ID3D12GraphicsCommandList1* m_D3D12GraphicsCommandList1 = nullptr;

        std::vector<D3D12_RESOURCE_BARRIER> m_D3D12Barriers;
    };

    class CopyCmdList : public CmdList
    {
    public:
        friend class GraphicsCmdQueue;

        using CmdList::CmdList;

        void CopyResource(const Resource& destResource, const Resource& sourceResource);
        void CopyBufferRegion(const Buffer& destBuffer, uint64_t destOffsetInBytes, const Buffer& sourceBuffer, uint64_t sourceOffsetInBytes, uint64_t dataSizeInBytes);
        void CopyTextureRegion(const Texture& destTexture, uint32_t destSubResourceIndex, const Texture& sourceTexture, uint32_t sourceSubresourceIndex);

        void UploadToBuffer(Buffer& destBuffer, std::span<const std::byte> data, uint64_t destOffsetInBytes);

        template <typename T>
        void UploadToBuffer(Buffer& destBuffer, std::span<const T> elements, uint32_t offsetElement = 0)
        {
            UploadToBuffer(destBuffer, std::as_bytes(elements), offsetElement * sizeof(T));
        }

        void UploadToTexture(Texture& texture, const std::vector<SubResourceData>& subResources);
        void UploadToTexture(Texture& texture, std::span<const std::byte> data);

    private:
        void SetUploadBuffer(Buffer& uploadBuffer);
        uint64_t AllocateInUploadBuffer(uint64_t sizeInBytes, uint64_t alignmentInBytes = 0);

    private:
        Buffer* m_UploadBuffer = nullptr;
        uint64_t m_UploadBufferOffsetInBytes = 0;
    };

    class ComputeCmdList : public CopyCmdList
    {
    public:
        explicit ComputeCmdList(Device& device);
        ~ComputeCmdList() override;

        // Ref: https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing
        // D3D12_COMMAND_LIST_TYPE_DIRECT and D3D12_COMMAND_LIST_TYPE_COMPUTE always support timestamps
        void SetTimestamp(const QueryHeap& timestampQueryHeap, uint32_t index);
        void ResolveTimestamps(const QueryHeap& timestampQueryHeap, const Buffer& readbackBuffer, uint64_t readbackOffsetInBytes);

        void SetComputeCbv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);
        void SetComputeSrv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);

        void SetComputeRootConstant(uint32_t rootIndex, uint32_t value);
        void SetComputeRootResource(uint32_t rootIndex, const Descriptor& viewDescriptor);

        void SetComputePso(const ComputePso& pso);

        void ClearUnorderedAccess(const Resource& resource, const Descriptor& viewDescriptor, const DirectX::XMFLOAT4& color);

        void Dispatch(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadGroupSize);

        // RayTracing
        void BuildRayTracingAccelerationStructure(const RayTracing_AcclerationStructure& accelerationStructure);

        void SetRayTracingPso(const RayTracing_Pso& pso);
        void DispatchRays(const RayTracing_ShaderTable& shaderTable, const DirectX::XMUINT3 dimenions);

    protected:
        ID3D12GraphicsCommandList4* m_D3D12GraphicsCommandList4 = nullptr;
    };

    class GraphicsCmdList : public ComputeCmdList
    {
    public:
        explicit GraphicsCmdList(Device& device);
        ~GraphicsCmdList() override;

        void SetGraphicsCbv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);
        void SetGraphicsSrv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);

        void SetGraphicsRootConstant(uint32_t rootIndex, uint32_t value);
        void SetGraphicsRootResource(uint32_t rootIndex, const Descriptor& viewDescriptor);

        void SetVertexPso(const VertexPso& pso);

        void SetVertexBuffer(const Buffer& vertexBuffer);
        void SetIndexBuffer(const Buffer& indexBuffer);

        void SetPrimitiveTopology(PrimitiveTopology primitiveTopology);
        void SetViewport(const Viewport& viewport);
        void SetScissorRect(const ScissorRect& scissorRect);

        void SetBlendFactor(const DirectX::XMFLOAT4& color);

        void SetRenderTargets(const std::vector<Descriptor>& rtvs, const Descriptor* dsv = nullptr);

        void ClearRenderTarget(const Texture& renderTarget, std::optional<DirectX::XMFLOAT4> overrideClearColor = std::nullopt);
        void ClearDepthStencil(const Texture& depthStencil);

        void DrawVertexed(uint32_t vertexCount, uint32_t instanceCount = 1);
        void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount = 1);

        // Mesh shaders
        void SetMeshPso(const MeshPso& pso);
        void DispatchMesh(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadGroupSize = DirectX::XMUINT3{ 1, 1, 1 });

    protected:
        ID3D12GraphicsCommandList6* m_D3D12GraphicsCommandList6 = nullptr;
    };

    class ScopedResourceBarriers
    {
    public:
        ScopedResourceBarriers(CmdList& cmdList, std::span<const ResourceBarrierVariant> resourceBarriers);
        ~ScopedResourceBarriers();

    private:
        CmdList& m_CmdList;

        std::vector<TransitionBarrier> m_SwappedTransitionBarriers;
    };

    class ScopedGpuEvent
    {
    public:
        explicit ScopedGpuEvent(CmdList& cmdList, std::string_view name);
        ~ScopedGpuEvent();

    private:
        ID3D12GraphicsCommandList* m_D3D12GraphicsCommandList = nullptr;
    };

}

#define BenzinScopedResourceBarriers(cmdList, ...) \
    const benzin::ScopedResourceBarriers BenzinUniqueVariableName(scopedResourceBarriers) \
    { \
        cmdList, \
        std::to_array<benzin::ResourceBarrierVariant>({ __VA_ARGS__ }), \
    }

#define BenzinGpuEvent(cmdList, name) \
    const benzin::ScopedGpuEvent BenzinUniqueVariableName(_scopedGpuEvent){ cmdList, name }
