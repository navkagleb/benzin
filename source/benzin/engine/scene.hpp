#pragma once

#include "benzin/engine/camera.hpp"
#include "benzin/engine/resource_loader.hpp"

namespace benzin
{

    class Device;
    class Texture;
    class Buffer;

    struct MeshCollection
    {
        std::vector<MeshData> Meshes;
        std::vector<MeshNode> MeshNodes;
        std::vector<MeshInstance> MeshInstances;

        std::vector<Material> Materials;
    };

    struct MeshCollectionGPUStorage
    {
        std::unique_ptr<Buffer> VertexBuffer;
        std::unique_ptr<Buffer> IndexBuffer;
        std::unique_ptr<Buffer> MeshInfoBuffer;
        std::unique_ptr<Buffer> MeshNodeBuffer;
        std::unique_ptr<Buffer> MeshInstanceBuffer;
        std::unique_ptr<Buffer> MaterialBuffer;
    };

    class Scene
    {
    public:
        explicit Scene(Device& device);

    public:
        auto& GetCamera() { return m_Camera; }

        const auto& GetMeshCollection(uint32_t index) const { return m_MeshCollections[index]; };
        const auto& GetMeshCollectionGPUStorage(uint32_t index) const { return m_MeshCollectionGPUStorages[index]; }

        auto GetActivePointLightCount() const { return m_ActivePointLightCount; };
        const auto& GetPointLightBuffer() const { return m_PointLightBuffer; }

        auto& GetEntityRegistry() { return m_EntityRegistry; }
        const auto& GetEntityRegistry() const { return m_EntityRegistry; }

    public:
        void OnUpdate();

        uint32_t PushMeshCollection(MeshCollectionResource&& meshCollectionResource);

        void UploadMeshCollections();

    private:
        void OnTransformComponentConstuct(entt::registry& registry, entt::entity entityHandle);

        void PushAndUploadTextures(std::span<const TextureImage> textureImages);
        void UploadAllMeshData();
        void UploadAllMeshNodes();
        void UploadAllMeshInstances();
        void UploadAllTextures();
        void UploadAllMaterials();

    private:
        Device& m_Device;

        PerspectiveProjection m_PerspectiveProjection;
        Camera m_Camera{ m_PerspectiveProjection };

        std::vector<std::string> m_MeshCollectionDebugNames;
        std::vector<MeshCollection> m_MeshCollections;
        std::vector<MeshCollectionGPUStorage> m_MeshCollectionGPUStorages;

        std::vector<std::vector<std::byte>> m_TexturesData;
        std::vector<std::unique_ptr<Texture>> m_Textures;

        uint32_t m_ActivePointLightCount = 0;
        std::unique_ptr<Buffer> m_PointLightBuffer;

        entt::registry m_EntityRegistry;
    };

} // namespace benzin
