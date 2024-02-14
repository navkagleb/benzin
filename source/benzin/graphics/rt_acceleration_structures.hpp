#pragma once

#include "benzin/graphics/buffer.hpp"

namespace benzin
{

    class Device;

} // namespace benzin

namespace benzin::rt
{

    struct AccelerationStructureSizes
    {
        uint64_t BufferSizeInBytes = 0;
        uint64_t ScratchResourceSizeInBytes = 0;
    };

    struct AccelerationStructureCreation
    {
        std::string_view DebugName;

        AccelerationStructureSizes Sizes;
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS D3D12BuildInputs;
    };

    class AccelerationStructure
    {
    protected:
        AccelerationStructure() = delete;
        explicit AccelerationStructure(Device& device);

    public:
        const auto& GetD3D12BuildInputs() const { return m_D3D12BuildInputs; }

        auto& GetBuffer() { return m_Buffer; }
        const auto& GetBuffer() const { return m_Buffer; }

        auto& GetScratchResource() { return m_ScratchResource; }
        const auto& GetScratchResource() const { return m_ScratchResource; }

    protected:
        void Create(const AccelerationStructureCreation& creation);

    protected:
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS m_D3D12BuildInputs;

        Buffer m_Buffer;
        Buffer m_ScratchResource;
    };

    struct TriangledGeometry
    {
        const Buffer& VertexBuffer;
        const Buffer& IndexBuffer;

        uint32_t VertexOffset = 0;
        uint32_t IndexOffset = 0;

        uint32_t VertexCount = g_InvalidIndex<uint32_t>;
        uint32_t IndexCount = g_InvalidIndex<uint32_t>;
    };

    struct ProceduralGeometry
    {
        const Buffer& AABBBuffer;
        uint32_t AABBIndex = 0;
    };

    using GeometryVariant = std::variant<TriangledGeometry, ProceduralGeometry>;

    struct BottomLevelAccelerationStructureCreation
    {
        std::string_view DebugName;
        std::span<const GeometryVariant> Geometries;
    };

    class BottomLevelAccelerationStructure : public AccelerationStructure
    {
    public:
        BottomLevelAccelerationStructure(Device& device, const BottomLevelAccelerationStructureCreation& creation);

    private:
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_D3D12GeometryDescs;
    };

    struct TopLevelInstance
    {
        const BottomLevelAccelerationStructure& BottomLevelAccelerationStructure;

        uint32_t HitGroupIndex = 0;
        DirectX::XMMATRIX Transform = DirectX::XMMatrixIdentity();
    };

    struct TopLevelAccelerationStructureCreation
    {
        std::string_view DebugName;
        std::span<const TopLevelInstance> Instances;
    };

    class TopLevelAccelerationStructure : public AccelerationStructure
    {
    public:
        TopLevelAccelerationStructure(Device& device, const TopLevelAccelerationStructureCreation& creation);

    private:
        std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_D3D12InstanceDescs;
        Buffer m_InstanceBuffer;
    };

} // namespace benzin::rt
