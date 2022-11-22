#pragma once

#include "spieler/graphics/buffer.hpp"

namespace spieler
{

    class Device;
    class PipelineState;
    class RootSignature;

    class BufferResource;
    class TextureResource;

    class TextureShaderResourceView;
    class TextureUnorderedAccessView;
    class TextureRenderTargetView;
    class TextureDepthStencilView;

    struct TransitionResourceBarrier;

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
        GraphicsCommandList(Device& device);

    public:
        ID3D12GraphicsCommandList* GetDX12GraphicsCommandList() const { return m_DX12GraphicsCommandList.Get(); }

	public:
        void Close();
        void Reset(const PipelineState* const pso = nullptr);

        // UploadBuffer
        uint64_t AllocateInUploadBuffer(uint64_t size, uint64_t alignment = 0);

        // Commands
        void SetDescriptorHeap(const DescriptorHeap& descriptorHeap);

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

        void SetGraphicsDescriptorTable(uint32_t rootParameterIndex, const TextureShaderResourceView& firstSRV);
        void SetComputeDescriptorTable(uint32_t rootParameterIndex, const TextureShaderResourceView& firstSRV);
        void SetComputeDescriptorTable(uint32_t rootParameterIndex, const TextureUnorderedAccessView& firstUAV);

        void SetRenderTarget(const TextureRenderTargetView& rtv);
        void SetRenderTarget(const TextureRenderTargetView& rtv, const TextureDepthStencilView& dsv);

        void ClearRenderTarget(const TextureRenderTargetView& rtv, const DirectX::XMFLOAT4& color);
        void ClearDepthStencil(const TextureDepthStencilView& dsv, float depth, uint8_t stencil);

        void SetResourceBarrier(const TransitionResourceBarrier& barrier);
        void SetStencilReferenceValue(uint8_t referenceValue);

        void UploadToBuffer(BufferResource& buffer, const void* data, uint64_t size);
        void UploadToTexture(TextureResource& texture, std::vector<SubresourceData>& subresources);
        void CopyResource(Resource& to, Resource& from);

        void DrawVertexed(uint32_t vertexCount, uint32_t startVertexLocation, uint32_t instanceCount = 1);
        void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount = 1);

        void Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ);

	private:
        ComPtr<ID3D12CommandAllocator> m_DX12CommandAllocator;
        ComPtr<ID3D12GraphicsCommandList> m_DX12GraphicsCommandList;

        std::shared_ptr<BufferResource> m_UploadBuffer;
        uint64_t m_UploadBufferOffset{ 0 };
	};

} // namespace spieler
