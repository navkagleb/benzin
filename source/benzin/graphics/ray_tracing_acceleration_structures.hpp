#pragma once

namespace benzin
{

    class Device;
    class Buffer;

    class RayTracing_AcclerationStructure
    {
    public:
        virtual ~RayTracing_AcclerationStructure();

    public:
        const auto& GetD3D12BuildInputs() const { return m_D3D12BuildInputs; }
        const auto* GetBuffer() const { return m_Buffer.get(); }
        const auto* GetScratchResource() const { return m_ScratchResource.get(); }

        uint64_t GetGpuVirtualAddress() const;

    protected:
        void AllocateBuffers(Device& device, std::string_view debugName, const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& d3d12BuildInputs);
        virtual void AllocateBuffers(Device& device, std::string_view debugName) = 0;

    protected:
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS m_D3D12BuildInputs{};

        std::unique_ptr<Buffer> m_Buffer;
        std::unique_ptr<Buffer> m_ScratchResource;
    };

    class RayTracing_Blas : public RayTracing_AcclerationStructure
    {
    public:
        struct Geometry
        {
            const Buffer& VertexBuffer;
            const Buffer& IndexBuffer;

            uint32_t VertexOffset = 0;
            uint32_t IndexOffset = 0;

            uint32_t VertexCount = g_InvalidUnsigned<uint32_t>;
            uint32_t IndexCount = g_InvalidUnsigned<uint32_t>;

            uint64_t TransformGpuAddress = 0;
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
            const RayTracing_Blas& Blas;

            uint32_t HitGroupIndex = 0;
            DirectX::XMMATRIX Transform = DirectX::XMMatrixIdentity();
        };

        RayTracing_Tlas() = default;
        RayTracing_Tlas(RayTracing_Tlas&& other) noexcept;

        void AddInstance(const Instance& instance);
        void ResetInstances(uint32_t reservedInstanceCount = 0);

        void AllocateBuffers(Device& device, std::string_view debugName) override;

    private:
        void AllocateInstanceBuffer(Device& device, std::string_view debugView);

    private:
        std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_D3D12InstanceDescs;

        std::unique_ptr<Buffer> m_InstanceBuffer;
    };

}
