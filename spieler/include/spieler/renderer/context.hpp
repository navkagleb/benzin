#pragma once

#include "spieler/renderer/fence.hpp"
#include "spieler/renderer/resource_barrier.hpp"
#include "spieler/renderer/types.hpp"

namespace spieler::renderer
{

    class Device;
    class Resource;
    class UploadBuffer;
    class PipelineState;
    class RootSignature;
    class Texture2DResource;

    class RenderTargetView;
    class DepthStencilView;

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
        SPIELER_NON_COPYABLE(Context)

    public:
        explicit Context(Device& device);

    public:
        ComPtr<ID3D12GraphicsCommandList>& GetNativeCommandList() { return m_CommandList; }
        ComPtr<ID3D12CommandQueue>& GetNativeCommandQueue() { return m_CommandQueue; }

        void SetViewport(const Viewport& viewport);
        void SetScissorRect(const ScissorRect& scissorRect);

        void SetPipelineState(const PipelineState& pso);
        void SetGraphicsRootSignature(const RootSignature& rootSignature);
        void SetComputeRootSignature(const RootSignature& rootSignature);

        void SetRenderTarget(const RenderTargetView& rtv);
        void SetRenderTarget(const RenderTargetView& rtv, const DepthStencilView& dsv);

        void ClearRenderTarget(const RenderTargetView& rtv, const DirectX::XMFLOAT4& color);
        void ClearDepthStencil(const DepthStencilView& dsv, float depth, uint8_t stencil);

        void SetResourceBarrier(const TransitionResourceBarrier& barrier);
        void SetPrimitiveTopology(PrimitiveTopology primitiveTopology);
        void SetStencilReferenceValue(uint8_t referenceValue);

        void CopyBuffer(const void* data, uint64_t size, UploadBuffer& from, Resource& to);
        void Copy(std::vector<D3D12_SUBRESOURCE_DATA>& subresources, uint32_t alingment, UploadBuffer& from, Resource& to);

        bool CloseCommandList();
        bool ExecuteCommandList(bool isNeedToFlushCommandQueue = false);
        bool FlushCommandQueue();
        bool ResetCommandList(const PipelineState* pso = nullptr);
        bool ResetCommandAllocator();

    private:
        bool Init(Device& device);
        bool InitCommandAllocator(Device& device);
        bool InitCommandList(Device& device);
        bool InitCommandQueue(Device& device);

    private:
        ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
        ComPtr<ID3D12GraphicsCommandList> m_CommandList;
        ComPtr<ID3D12CommandQueue> m_CommandQueue;

        Fence m_Fence;
    };

} // namespace spieler::renderer