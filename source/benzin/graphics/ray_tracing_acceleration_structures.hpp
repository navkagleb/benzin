#pragma once

namespace benzin
{

    class Device;
    class Buffer;

    class RayTracing_AcclerationStructure
    {
    public:
        RayTracing_AcclerationStructure() = default;
        RayTracing_AcclerationStructure(RayTracing_AcclerationStructure&&) = default;
        virtual ~RayTracing_AcclerationStructure();

        const auto& GetD3D12BuildInputs() const { return m_D3D12BuildInputs; }
        const auto* GetBuffer() const { return m_Buffer.get(); }
        const auto* GetScratchResource() const { return m_ScratchResource.get(); }

        uint64_t GetGpuVirtualAddress() const;

        virtual void AllocateBuffers(Device& device, std::string_view debugName) = 0;

    protected:
        void AllocateBuffers(Device& device, std::string_view debugName, const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& d3d12BuildInputs);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS m_D3D12BuildInputs = {};

        std::unique_ptr<Buffer> m_Buffer;
        std::unique_ptr<Buffer> m_ScratchResource;
    };

    class RayTracing_Blas : public RayTracing_AcclerationStructure
    {
    public:
        struct Geometry
        {
            const Buffer& m_VertexBuffer;
            const Buffer& m_IndexBuffer;

            uint32_t m_VertexOffset = 0;
            uint32_t m_VertexCount = 0;

            uint32_t m_IndexOffset = 0;
            uint32_t m_IndexCount = 0;

            uint64_t m_TransformGpuAddress = 0;
        };

        explicit RayTracing_Blas(uint32_t reservedGeometryCount = 0);

        void AddGeometry(const Geometry& geometry);
        void AllocateBuffers(Device& device, std::string_view debugName) override;

    private:
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_D3D12GeometryDescs;
    };

    class RayTracing_Tlas : public RayTracing_AcclerationStructure
    {
    public:
        struct Instance
        {
            uint64_t m_BlasGpuVirtualAddress = 0;
            DirectX::XMMATRIX m_LocalToWorld = DirectX::XMMatrixIdentity();
        };

        RayTracing_Tlas() = default;

        auto* GetInstanceBuffer() const { return m_InstanceBuffer.get(); }

        void AddInstance(const Instance& instance);
        void ResetInstances(uint32_t reservedInstanceCount = 0);

        void AllocateBuffers(Device& device, std::string_view debugName) override;

    private:
        void AllocateInstanceBuffer(Device& device, std::string_view debugView);

        std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_D3D12InstanceDescs;
        std::unique_ptr<Buffer> m_InstanceBuffer;
    };

}
