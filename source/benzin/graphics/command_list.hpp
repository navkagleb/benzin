#pragma once

#include "benzin/graphics/common.hpp"
#include "benzin/graphics/resource.hpp"

namespace benzin
{

    class Buffer;
    class ComputePso;
    class Descriptor;
    class GraphicsPso;
    class QueryHeap;
    class RayTracing_AcclerationStructure;
    class RayTracing_Pso;
    class RayTracing_ShaderTable;
    class Texture;

    struct SubResourceData;

    enum class UnifiedRootParameter;

    class GraphicsCommandList
    {
    public:
        friend class GraphicsCommandQueue;
        friend class ScopedResourceBarriers;

        explicit GraphicsCommandList(Device& device);
        ~GraphicsCommandList();

        BenzinDefineNonCopyable(GraphicsCommandList);
        BenzinDefineNonMoveable(GraphicsCommandList);

    public:
        auto* GetD3D12GraphicsCommandList() const { return m_D3D12GraphicsCommandList; }

    public:
        // TODO: This part is for CopyCommandList?
        void CopyResource(const Resource& destination, const Resource& source);

        void UploadToBuffer(Buffer& buffer, std::span<const std::byte> data, Bytes64 offset);
        
        template <typename T>
        void UploadToBuffer(Buffer& buffer, std::span<const T> elements, size_t offsetElement = 0)
        {
            UploadToBuffer(buffer, std::as_bytes(elements), Bytes64{ offsetElement * sizeof(T) });
        }

        void UploadToTexture(Texture& texture, const std::vector<SubResourceData>& subResources);
        void UploadToTextureTopMip(Texture& texture, std::span<const std::byte> data);

        // Compute
        void SetComputeCbv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);
        void SetComputeSrv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);

        void SetComputeRootConstant(uint32_t rootIndex, uint32_t value);
        void SetComputeRootResource(uint32_t rootIndex, const Descriptor& viewDescriptor);

        void SetComputePso(const ComputePso& pso);

        void ClearUnorderedAccess(const Resource& resource, const Descriptor& viewDescriptor, const DirectX::XMFLOAT4& color);

        void Dispatch(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadGroupSize);

        // Ref: https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing
        // D3D12_COMMAND_LIST_TYPE_DIRECT and D3D12_COMMAND_LIST_TYPE_COMPUTE always support timestamps
        void SetTimestamp(const QueryHeap& timestampQueryHeap, uint32_t index);
        void ResolveTimestamps(const QueryHeap& timestampQueryHeap, const Buffer& readbackBuffer, uint64_t readbackBufferOffset);

        // Graphics 
        void SetGraphicsCbv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);
        void SetGraphicsSrv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);

        void SetGraphicsRootConstant(uint32_t rootIndex, uint32_t value);
        void SetGraphicsRootResource(uint32_t rootIndex, const Descriptor& viewDescriptor);

        void SetGraphicsPso(const GraphicsPso& pso);

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

        // RayTracing
        void BuildRayTracingAccelerationStructure(const RayTracing_AcclerationStructure& accelerationStructure);

        void SetRayTracingPso(const RayTracing_Pso& pso);

        void DispatchRays(const RayTracing_ShaderTable& shaderTable, const DirectX::XMUINT3 dimenions);

    private:
        void SetUploadBuffer(Buffer& uploadBuffer);
        Bytes64 AllocateInUploadBuffer(Bytes64 size, Bytes64 alignment = 0);

    private:
        ID3D12GraphicsCommandList4* m_D3D12GraphicsCommandList = nullptr;

        Buffer* m_UploadBuffer = nullptr;
        Bytes64 m_UploadBufferOffset = 0;
    };

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

    class ResourceBarriers
    {
    public:
        ResourceBarriers(GraphicsCommandList& commandList, const std::vector<ResourceBarrierVariant>& resourceBarriers, bool isScoped);
        ~ResourceBarriers();

    private:
        void SetD3D12Barriers(std::span<const D3D12_RESOURCE_BARRIER> d3d12Barriers) const;

    private:
        GraphicsCommandList& m_CommandList;

        std::vector<TransitionBarrier> m_SwappedTransitionBarriers;
        bool m_IsScoped = false;
    };

    class ScopedGpuEvent
    {
    public:
        explicit ScopedGpuEvent(GraphicsCommandList& commandList, std::string_view name);
        ~ScopedGpuEvent();

    private:
        ID3D12GraphicsCommandList* m_D3D12GraphicsCommandList = nullptr;
    };

}

#define BenzinMakeResourceBarriers(commandList, ...) \
    const benzin::ResourceBarriers BenzinUniqueVariableName(scopedResourceBarriers) \
    { \
        commandList, \
        std::vector<benzin::ResourceBarrierVariant>{ __VA_ARGS__ }, \
        false, \
    }

#define BenzinMakeScopedResourceBarriers(commandList, ...) \
    const benzin::ResourceBarriers BenzinUniqueVariableName(scopedResourceBarriers) \
    { \
        commandList, \
        std::vector<benzin::ResourceBarrierVariant>{ __VA_ARGS__ }, \
        true, \
    }

#define BenzinGpuEvent(commandList, name) \
    const benzin::ScopedGpuEvent BenzinUniqueVariableName(_scopedGpuEvent){ commandList, name }
