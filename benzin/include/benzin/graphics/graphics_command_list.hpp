#pragma once

#include "benzin/graphics/resource.hpp"

namespace benzin
{

    struct Viewport
    {
        float X{ 0.0f };
        float Y{ 0.0f };
        float Width{ 0.0f };
        float Height{ 0.0f };
        float MinDepth{ 0.0f };
        float MaxDepth{ 1.0f };
    };

    struct ScissorRect
    {
        float X{ 0.0f };
        float Y{ 0.0f };
        float Width{ 0.0f };
        float Height{ 0.0f };
    };

	class GraphicsCommandList
	{
    public:
        BENZIN_NON_COPYABLE(GraphicsCommandList)
        BENZIN_NON_MOVEABLE(GraphicsCommandList)
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12GraphicsCommandList, "GraphicsCommandList")

    public:
        GraphicsCommandList(Device& device, const std::string& debugName = {});
        ~GraphicsCommandList();

    public:
        ID3D12GraphicsCommandList* GetD3D12GraphicsCommandList() const { return m_D3D12GraphicsCommandList; }

	public:
        void Close();
        void Reset(const PipelineState* const pso = nullptr);

        // Commands
        void SetDescriptorHeaps(const DescriptorManager& descriptorManager);

        void IASetVertexBuffer(const BufferResource* vertexBuffer);
        void IASetIndexBuffer(const BufferResource* indexBuffer);
        void IASetPrimitiveTopology(PrimitiveTopology primitiveTopology);

        void SetViewport(const Viewport& viewport);
        void SetScissorRect(const ScissorRect& scissorRect);

        void SetPipelineState(const PipelineState& pso);
        void SetGraphicsRootSignature(const RootSignature& rootSignature);
        void SetComputeRootSignature(const RootSignature& rootSignature);

        void SetCompute32BitConstants(uint32_t rootParameterIndex, const void* data, uint64_t count, uint64_t offsetCount);

        void SetGraphicsRawConstantBuffer(uint32_t rootParameterIndex, const BufferResource& bufferResource, uint32_t beginElement = 0);
        void SetGraphicsRawShaderResource(uint32_t rootParameterIndex, const BufferResource& bufferResource, uint32_t beginElement = 0);

        void SetGraphicsDescriptorTable(uint32_t rootParameterIndex, const Descriptor& firstDescriptor);
        void SetComputeDescriptorTable(uint32_t rootParameterIndex, const Descriptor& firstDescriptor);

        void SetRenderTarget(const Descriptor* rtv, const Descriptor* dsv = nullptr);

        void ClearRenderTarget(const Descriptor& rtv, const DirectX::XMFLOAT4& color);
        void ClearDepthStencil(const Descriptor& dsv, float depth, uint8_t stencil);

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
        ID3D12CommandAllocator* m_D3D12CommandAllocator{ nullptr };
        ID3D12GraphicsCommandList* m_D3D12GraphicsCommandList{ nullptr };

        std::shared_ptr<BufferResource> m_UploadBuffer;
        uint64_t m_UploadBufferOffset{ 0 };
	};

} // namespace benzin
