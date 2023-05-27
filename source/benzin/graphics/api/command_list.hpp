#pragma once

#include "benzin/graphics/api/buffer.hpp"
#include "benzin/graphics/api/texture.hpp"
#include "benzin/graphics/mapped_data.hpp"

namespace benzin
{

    class PipelineState;

    class CommandList
    {
    public:
        BENZIN_NON_COPYABLE(CommandList)
        BENZIN_NON_MOVEABLE(CommandList)
        BENZIN_DX_DEBUG_NAME_IMPL(m_D3D12GraphicsCommandList)

    public:
        CommandList(Device& device, D3D12_COMMAND_LIST_TYPE d3d12CommandListType);
        virtual ~CommandList();

    public:
        ID3D12GraphicsCommandList1* GetD3D12GraphicsCommandList() const { return m_D3D12GraphicsCommandList; }

    public:
        void SetResourceBarrier(Resource& resource, Resource::State resourceStateAfter);
        void CopyResource(Resource& to, Resource& from);

    protected:
        ID3D12GraphicsCommandList1* m_D3D12GraphicsCommandList{ nullptr };
    };

    class CopyCommandList : public CommandList
    {
    public:
        friend class CopyCommandQueue;

    public:
        CopyCommandList() = default;
        explicit CopyCommandList(Device& device);

    public:
        template <typename T>
        void UpdateBuffer(BufferResource& buffer, std::span<const T> data, size_t startElement = 0) { UpdateBuffer(buffer, std::span{ reinterpret_cast<const std::byte*>(data.data()), data.size_bytes() }, startElement * sizeof(T)); }
        void UpdateBuffer(BufferResource& buffer, std::span<const std::byte> data, size_t offsetInBytes);

        void UpdateTexture(TextureResource& texture, const std::vector<SubResourceData>& subResources);

        void UpdateTextureTopMip(TextureResource& texture, const std::byte* data);

    private:
        bool IsValid() const { return m_UploadBuffer == nullptr; }

        void CreateUploadBuffer(Device& device, uint32_t size);
        void ReleaseUploadBuffer();

        size_t AllocateInUploadBuffer(size_t size, size_t alignment = 0);

    private:
        std::unique_ptr<BufferResource> m_UploadBuffer;
        size_t m_UploadBufferOffset{ 0 };
    };

    class ComputeCommandList : public CommandList
    {
    public:
        ComputeCommandList() = default;
        explicit ComputeCommandList(Device& device);

    public:
        template <typename T> requires std::is_enum_v<T>
        void SetRootConstant(T rootIndexEnum, uint32_t value) { SetRootConstant(magic_enum::enum_integer(rootIndexEnum), value); }
        void SetRootConstant(uint32_t rootIndex, uint32_t value);

        template <typename T> requires std::is_enum_v<T>
        void SetRootConstantBuffer(T rootIndexEnum, const Descriptor& cbv) { SetRootConstantBuffer(magic_enum::enum_integer(rootIndexEnum), cbv); }
        void SetRootConstantBuffer(uint32_t rootIndex, const Descriptor& cbv);

        template <typename T> requires std::is_enum_v<T>
        void SetRootShaderResource(T rootIndexEnum, const Descriptor& srv) { SetRootShaderResource(magic_enum::enum_integer(rootIndexEnum), srv); }
        void SetRootShaderResource(uint32_t rootIndex, const Descriptor& srv);

        template <typename T> requires std::is_enum_v<T>
        void SetRootUnorderedAccess(T rootIndexEnum, const Descriptor& uav) { SetRootShaderResource(magic_enum::enum_integer(rootIndexEnum), uav); }
        void SetRootUnorderedAccess(uint32_t rootIndex, const Descriptor& uav);

        void SetPipelineState(const PipelineState& pso); // TODO: Remove duplicate ComputeCommandList

        void Dispatch(const DirectX::XMUINT3& dimension, const DirectX::XMUINT3& threadPerGroupCount);
    };

	class GraphicsCommandList : public CommandList
	{
    public:
        GraphicsCommandList() = default;
        explicit GraphicsCommandList(Device& device);

	public:
        template <typename T> requires std::is_enum_v<T>
        void SetRootConstant(T rootIndexEnum, uint32_t value) { SetRootConstant(magic_enum::enum_integer(rootIndexEnum), value); }
        void SetRootConstant(uint32_t rootIndex, uint32_t value);

        template <typename T> requires std::is_enum_v<T>
        void SetRootConstantBuffer(T rootIndexEnum, const Descriptor& cbv) { SetRootConstantBuffer(magic_enum::enum_integer(rootIndexEnum), cbv); }
        void SetRootConstantBuffer(uint32_t rootIndex, const Descriptor& cbv);

        template <typename T> requires std::is_enum_v<T>
        void SetRootShaderResource(T rootIndexEnum, const Descriptor& srv) { SetRootShaderResource(magic_enum::enum_integer(rootIndexEnum), srv); }
        void SetRootShaderResource(uint32_t rootIndex, const Descriptor& srv);

        void SetPipelineState(const PipelineState& pso); // TODO: Remove duplicate ComputeCommandList

        void SetPrimitiveTopology(PrimitiveTopology primitiveTopology);

        void SetViewport(const Viewport& viewport);
        void SetScissorRect(const ScissorRect& scissorRect);

        void SetRenderTargets(const std::vector<Descriptor>& rtvs, const Descriptor* dsv = nullptr);

        void ClearRenderTarget(const Descriptor& rtv, const TextureResource::ClearColor& clearColor = {});
        void ClearDepthStencil(const Descriptor& dsv, const TextureResource::ClearDepthStencil& clearDepthStencil = {});

        void DrawVertexed(uint32_t vertexCount, uint32_t instanceCount = 1);
        void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t instanceCount = 1);
	};

} // namespace benzin
