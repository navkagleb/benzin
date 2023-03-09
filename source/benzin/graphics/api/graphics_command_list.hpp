#pragma once

#include "benzin/graphics/api/resource.hpp"

namespace benzin
{

	class GraphicsCommandList
	{
    public:
        BENZIN_NON_COPYABLE(GraphicsCommandList)
        BENZIN_NON_MOVEABLE(GraphicsCommandList)
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12GraphicsCommandList, "GraphicsCommandList")

    public:
        GraphicsCommandList(Device& device, ID3D12CommandAllocator* d3d12CommandAllocator, std::string_view debugName);
        ~GraphicsCommandList();

    public:
        ID3D12GraphicsCommandList* GetD3D12GraphicsCommandList() const { return m_D3D12GraphicsCommandList; }

	public:
        void IASetVertexBuffer(const BufferResource* vertexBuffer);
        void IASetIndexBuffer(const BufferResource* indexBuffer);
        void IASetPrimitiveTopology(PrimitiveTopology primitiveTopology);

        void SetViewport(const Viewport& viewport);
        void SetScissorRect(const ScissorRect& scissorRect);

        void SetPipelineState(const PipelineState& pso);

        void SetRootConstant(uint32_t constantIndex, uint32_t value, bool isCompute = false);

        void SetRenderTarget(const Descriptor* rtv, const Descriptor* dsv = nullptr);

        void ClearRenderTarget(const Descriptor& rtv);
        void ClearDepthStencil(const Descriptor& dsv);

        void SetResourceBarrier(Resource& resource, Resource::State resourceStateAfter);
        void SetStencilReferenceValue(uint8_t referenceValue);

        void UploadToBuffer(BufferResource& buffer, const void* data, uint64_t size);
        void UploadToTexture(TextureResource& texture, const std::vector<SubResourceData>& subResources);
        void CopyResource(Resource& to, Resource& from);

        void DrawVertexed(uint32_t vertexCount, uint32_t startVertexLocation, uint32_t instanceCount = 1);
        void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount = 1);

        void Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ);

    private:
        uint64_t AllocateInUploadBuffer(uint64_t size, uint64_t alignment = 0);

	private:
        ID3D12GraphicsCommandList* m_D3D12GraphicsCommandList{ nullptr };

        std::shared_ptr<BufferResource> m_UploadBuffer;
        uint64_t m_UploadBufferOffset{ 0 };
	};

} // namespace benzin
