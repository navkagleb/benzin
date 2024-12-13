#pragma once

#include "benzin/engine/camera.hpp"
#include "benzin/engine/resource_loader.hpp"

#include <shaders/joint/constant_buffer_types.hpp>

namespace benzin
{

    class Buffer;
    class Descriptor;
    class Device;
    class Texture;
    class TickTimer;

    class BottomLevelAccelerationStructure;
    class TopLevelAccelerationStructure;

    template <typename>
    class ConstantBuffer;

    struct MeshCollection
    {
        std::vector<MeshData> Meshes;
        std::vector<Material> Materials;
        std::vector<MeshInstance> MeshInstances;

        auto GetFullMeshInstanceRange() const
        {
            return IndexRange32{ 0, (uint32_t)MeshInstances.size() };
        }
    };

    struct MeshCollectionGpuStorage
    {
        std::unique_ptr<Buffer> VertexBuffer;
        std::unique_ptr<Buffer> IndexBuffer;
        std::unique_ptr<Buffer> MeshInfoBuffer;
        std::unique_ptr<Buffer> MeshInstanceBuffer;
        std::unique_ptr<Buffer> MaterialBuffer;
    };

    struct SceneStats
    {
        uint32_t VertexCount = 0;
        uint32_t TriangleCount = 0;

        uint32_t MeshCount = 0;
        uint32_t MaterialCount = 0;
        uint32_t MeshInstanceCount = 0;
    };

    class Scene
    {
    private:
        struct MeshUnion
        {
            std::string DebugName;
            MeshCollection Collection;
            MeshCollectionGpuStorage GpuStorage;

            std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> BottomLevelASs;
        };

    public:
        explicit Scene(Device& device);
        ~Scene();

    public:
        auto& GetPerspectiveProjection() { return m_PerspectiveProjection; }
        const auto& GetPerspectiveProjection() const { return m_PerspectiveProjection; }

        auto& GetCamera() { return m_Camera; }
        const auto& GetCamera() const { return m_Camera; }

        const auto& GetStats() const { return m_Stats; }

        std::string_view GetMeshCollectionDebugName(uint32_t index) const { return m_MeshUnions[index].DebugName; }
        const auto& GetMeshCollection(uint32_t index) const { return m_MeshUnions[index].Collection; };
        const auto& GetMeshCollectionGpuStorage(uint32_t index) const { return m_MeshUnions[index].GpuStorage; }

        const TopLevelAccelerationStructure& GetActiveTopLevelAs() const;
        const Descriptor& GetPointLightBufferStructuredSrv() const;

        auto& GetEntityRegistry() { return m_EntityRegistry; }
        const auto& GetEntityRegistry() const { return m_EntityRegistry; }

        bool HasMeshes() const { return !m_MeshUnions.empty(); }

    public:
        void OnUpdate();

        uint32_t PushMeshCollection(MeshCollectionResource&& meshCollectionResource);

        void UploadMeshCollections();
        void BuildBottomLevelAccelerationStructures();
        void BuildTopLevelAccelerationStructure();

    private:
        std::unique_ptr<TopLevelAccelerationStructure>& GetActiveTopLevelAsPtr();

        void OnTransformComponentConstuct(entt::registry& registry, entt::entity entityHandle);

        void PushTextures(std::span<const TextureImage> textureImages);
        void PushBottomLevelAs(MeshUnion& meshUnion);

        void CreateTopLevelAs();

        void UploadAllMeshData();
        void UploadAllMeshInstances();
        void UploadAllTextures();
        void UploadAllMaterials();

        void UpdateStats(const MeshUnion& meshUnion);

    private:
        Device& m_Device;

        SceneStats m_Stats;

        PerspectiveProjection m_PerspectiveProjection;
        Camera m_Camera{ m_PerspectiveProjection };

        std::vector<MeshUnion> m_MeshUnions;

        std::vector<std::unique_ptr<TopLevelAccelerationStructure>> m_TopLevelAss;

        std::vector<std::vector<std::byte>> m_TexturesData;
        std::vector<std::unique_ptr<Texture>> m_Textures;

        std::unique_ptr<Buffer> m_PointLightBuffer;

        entt::registry m_EntityRegistry;
    };

} // namespace benzin
