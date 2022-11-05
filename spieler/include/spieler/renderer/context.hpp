#pragma once

#include "spieler/renderer/common.hpp"
#include "spieler/renderer/fence.hpp"
#include "spieler/renderer/buffer.hpp"
#include "spieler/renderer/resource_barrier.hpp"
#include "spieler/renderer/pipeline_state.hpp"

#include "spieler/renderer/buffer.hpp"
#include "spieler/renderer/texture.hpp"

namespace spieler::renderer
{

    class Device;
    class RootSignature;
    class DescriptorHeap;

    //class Resource;
    //class BufferResource;
    //class TextureResource;

    //class VertexBufferView;
    //class IndexBufferView;
    //class RenderTargetView;
    //class DepthStencilView;

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

    class Context
    {
    public:
        class CommandListScope
        {
        public:
            CommandListScope(Context& context, bool isNeedToFlushCommandQueue);
            ~CommandListScope();

        private:
            Context& m_Context;
            bool m_IsNeedToFlushCommandQueue{ false };
        };

    private:
        struct UploadBuffer
        {
            std::shared_ptr<BufferResource> Resource;
            uint64_t Offset{ 0 };

            uint64_t Allocate(uint64_t size, uint64_t alignment = 0);
        };

    public:
        SPIELER_NON_COPYABLE(Context)

    public:
        Context(Device& device, uint64_t uploadBufferSize);

    public:
        ID3D12GraphicsCommandList* GetDX12GraphicsCommandList() const { return m_DX12GraphicsCommandList.Get(); }
        ID3D12CommandQueue* GetDX12CommandQueue() const { return m_DX12CommandQueue.Get(); }

        void SetDescriptorHeap(const DescriptorHeap& descriptorHeap);

        void IASetVertexBuffer(const BufferResource* vertexBuffer);
        void IASetIndexBuffer(const BufferResource* indexBuffer);
        void IASetPrimitiveTopology(PrimitiveTopology primitiveTopology);

        void SetViewport(const Viewport& viewport);
        void SetScissorRect(const ScissorRect& scissorRect);

        void SetPipelineState(const PipelineState& pso);
        void SetGraphicsRootSignature(const RootSignature& rootSignature);
        void SetComputeRootSignature(const RootSignature& rootSignature);

        void SetGraphicsRawConstantBuffer(uint32_t rootParameterIndex, const BufferResource& bufferResource, uint32_t beginElement = 0);
        void SetGraphicsRawShaderResource(uint32_t rootParameterIndex, const BufferResource& bufferResource, uint32_t beginElement = 0);
        void SetGraphicsDescriptorTable(uint32_t rootParameterIndex, const TextureShaderResourceView& firstSRV);

        void SetRenderTarget(const TextureRenderTargetView& rtv);
        void SetRenderTarget(const TextureRenderTargetView& rtv, const TextureDepthStencilView& dsv);

        void ClearRenderTarget(const TextureRenderTargetView& rtv, const DirectX::XMFLOAT4& color);
        void ClearDepthStencil(const TextureDepthStencilView& dsv, float depth, uint8_t stencil);

        void SetResourceBarrier(const TransitionResourceBarrier& barrier);
        void SetStencilReferenceValue(uint8_t referenceValue);

        void UploadToBuffer(BufferResource& buffer, const void* data, uint64_t size);
        void UploadToTexture(TextureResource& texture, std::vector<SubresourceData>& subresources);

        void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount = 1);

        void CloseCommandList();
        void ExecuteCommandList(bool isNeedToFlushCommandQueue = false);
        void FlushCommandQueue();
        void ResetCommandList(const PipelineState& pso = PipelineState{});
        void ResetCommandAllocator();

    private:
        bool Init(Device& device);
        bool InitCommandAllocator(Device& device);
        bool InitCommandList(Device& device);
        bool InitCommandQueue(Device& device);

    private:
        ComPtr<ID3D12CommandAllocator> m_DX12CommandAllocator;
        ComPtr<ID3D12GraphicsCommandList> m_DX12GraphicsCommandList;
        ComPtr<ID3D12CommandQueue> m_DX12CommandQueue;

        Fence m_Fence;
        UploadBuffer m_UploadBuffer;
    };

} // namespace spieler::renderer
