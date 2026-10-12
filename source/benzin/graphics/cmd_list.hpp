#pragma once

#include <benzin/graphics/resource.hpp>
#include <benzin/graphics/texture.hpp>

namespace benzin
{

    class Buffer;
    class ComputePso;
    class Descriptor;
    class MeshPso;
    class RayTracing_AcclerationStructure;
    class RayTracing_Pso;
    class RayTracing_ShaderTable;
    class VertexPso;
    struct SubResourceData;

    enum class UnifiedRootParameter;

    class CmdList
    {
    public:
        explicit CmdList(Device& device);
        virtual ~CmdList();

        auto* GetD3D12GraphicsCommandList() const { return m_D3D12GraphicsCommandList1; }

        void AddTransitionBarrier(const Resource& resource, D3D12_RESOURCE_STATES d3d12StateAfter);
        void AddUavBarrier(const Resource& resource);
        void FlushBarriers();

    protected:
        ID3D12GraphicsCommandList1* m_D3D12GraphicsCommandList1 = nullptr;

        struct TransitionBarrier
        {
            const Resource* m_Resource = nullptr;
            D3D12_RESOURCE_STATES m_D3D12StateAfter;
        };

        std::vector<TransitionBarrier> m_DeferredTransitionBarriers;
        std::vector<const Resource*> m_DeferredUnorderedAccessBarriers;
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

        void UploadToTexture(Texture& texture, std::span<const SubResourceData> subResources);
        void UploadToTexture(Texture& texture, std::span<const std::byte> data);

    private:
        void SetUploadBuffer(Buffer& uploadBuffer);
        uint64_t AllocateInUploadBuffer(uint64_t sizeInBytes, uint32_t alignmentInBytes = 0);

    private:
        Buffer* m_UploadBuffer = nullptr;
        uint64_t m_UploadBufferOffsetInBytes = 0; // TODO: Replace with BufferWriter
    };

    class ComputeCmdList : public CopyCmdList
    {
    public:
        explicit ComputeCmdList(Device& device);
        ~ComputeCmdList() override;

        void SetComputeCbv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);
        void SetComputeSrv(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);
        void SetComputeUav(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);

        void SetComputeRootConstant(uint32_t rootIndex, uint32_t value);
        void SetComputeRootSrv(uint32_t rootIndex, const Buffer& buffer);
        void SetComputeRootSrv(uint32_t rootIndex, const Texture& texture, const TextureSrv& srv = {}, D3D12_RESOURCE_STATES d3d12ResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        void SetComputeRootUav(uint32_t rootIndex, const Buffer& buffer);
        void SetComputeRootUav(uint32_t rootIndex, const Texture& texture, const TextureUav& uav = {});

        void SetComputePso(const ComputePso& pso);

        void ClearUnorderedAccess(const Resource& resource, const Descriptor& uav, const DirectX::XMFLOAT4& color = {});

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
        void SetGraphicsUav(UnifiedRootParameter rootParameter, uint64_t gpuVirtualAddress);

        void SetGraphicsRootConstant(uint32_t rootIndex, uint32_t value);
        void SetGraphicsRootSrv(uint32_t rootIndex, const Buffer& buffer, D3D12_RESOURCE_STATES d3d12State);
        void SetGraphicsRootSrv(uint32_t rootIndex, const Texture& texture, D3D12_RESOURCE_STATES d3d12State);

        void SetVertexPso(const VertexPso& pso);

        void SetVertexBuffer(const Buffer& vertexBuffer);
        void SetIndexBuffer(const Buffer& indexBuffer);

        void AddRenderTarget(const Texture& texture, D3D12_RESOURCE_STATES d3d12State);
        void AddDepthStencil(const Texture& texture, D3D12_RESOURCE_STATES d3d12State);
        void SetRenderTargets();

        void ClearRenderTarget(const Texture& texture, std::optional<DirectX::XMFLOAT4> clearColor = std::nullopt);
        void ClearDepthStencil(const Texture& texture);

        void DrawVertexed(uint32_t vertexCount, uint32_t instanceCount = 1);
        void DrawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset, uint32_t instanceCount = 1);

        // Mesh shaders
        void SetMeshPso(const MeshPso& pso);
        void DispatchMesh(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadGroupSize = DirectX::XMUINT3{ 1, 1, 1 });

    private:
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_DeferredD3D12Rtvs;
        D3D12_CPU_DESCRIPTOR_HANDLE m_DeferredD3D12Dsv = {};

    protected:
        ID3D12GraphicsCommandList6* m_D3D12GraphicsCommandList6 = nullptr;
    };

    class ScopedGpuEvent
    {
    public:
        explicit ScopedGpuEvent(std::string_view name);
        ~ScopedGpuEvent();

        static void SetContext(Device& device);

    private:
        static inline Device* ms_Device = nullptr;
    };

}

#define BenzinGpuEvent(name) const benzin::ScopedGpuEvent BenzinUniqueVariableName(_scopedGpuEvent){ name }
