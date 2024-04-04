#pragma once

#include "benzin/graphics/common.hpp"
#include "benzin/graphics/resource.hpp"

namespace benzin
{

    class Buffer;
    class Descriptor;
    class PipelineState;
    class RtAccelerationStructure;
    class Texture;

    struct SubResourceData;

    struct TransitionBarrier
    {
        Resource& Resource;
        ResourceState StateAfter;
    };

    struct UnorderedAccessBarrier
    {
        Resource& Resource;
    };

    using ResourceBarrierVariant = std::variant<TransitionBarrier, UnorderedAccessBarrier>;

    class GraphicsCommandList
    {
    public:
        explicit GraphicsCommandList(Device& device);
        ~GraphicsCommandList();

        BenzinDefineNonCopyable(GraphicsCommandList);
        BenzinDefineNonMoveable(GraphicsCommandList);

    public:
        auto* GetD3D12GraphicsCommandList() const { return m_D3D12GraphicsCommandList; }

        void SetUploadBuffer(Buffer& uploadBuffer);

        void SetResourceBarrier(const ResourceBarrierVariant& resourceBarrier);
        void SetResourceBarriers(const std::vector<ResourceBarrierVariant>& resourceBarriers);

        void CopyResource(Resource& to, Resource& from);

        void UploadToBuffer(Buffer& buffer, std::span<const std::byte> data, size_t offsetInBytes);

        template <typename T>
        void UploadToBuffer(Buffer& buffer, std::span<const T> elements, size_t offsetElement = 0)
        {
            UploadToBuffer(buffer, std::as_bytes(elements), offsetElement * sizeof(T));
        }

        void UploadToTexture(Texture& texture, const std::vector<SubResourceData>& subResources);
        void UploadToTextureTopMip(Texture& texture, std::span<const std::byte> data);

        void SetRootConstant(uint32_t rootIndex, uint32_t value);
        void SetRootResource(uint32_t rootIndex, const Descriptor& viewDescriptor);

        void SetPipelineState(const PipelineState& pso);

        void SetPrimitiveTopology(PrimitiveTopology primitiveTopology);

        void SetViewport(const Viewport& viewport);
        void SetScissorRect(const ScissorRect& scissorRect);

        void SetRenderTargets(const std::vector<Descriptor>& rtvs, const Descriptor* dsv = nullptr);

        void ClearRenderTarget(const Descriptor& rtv, const DirectX::XMFLOAT4& color = g_DefaultClearColor);
        void ClearDepthStencil(const Descriptor& dsv, const DepthStencil& depthStencil = g_DefaultClearDepthStencil);

        void DrawVertexed(uint32_t vertexCount, uint32_t instanceCount = 1);
        void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount = 1);

        void Dispatch(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadPerGroupCount);

        void BuildRayTracingAccelerationStructure(const RtAccelerationStructure& accelerationStructure);

    private:
        uint64_t AllocateInUploadBuffer(uint64_t sizeInBytes, uint64_t alignmentInBytes = 0);

    private:
        ID3D12GraphicsCommandList4* m_D3D12GraphicsCommandList = nullptr;

        Buffer* m_UploadBuffer = nullptr;
        uint64_t m_UploadBufferOffset = 0;
    };

} // namespace benzin
