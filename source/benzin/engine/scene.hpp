#pragma once

#include "benzin/engine/camera.hpp"

namespace benzin
{

    struct Mesh;
    struct MeshGpuStorage;
    struct MeshResource;
    struct TextureImage;

    class Buffer;
    class Descriptor;
    class Device;
    class Texture;
    class TickTimer;

    template <typename>
    class ConstantBuffer;

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
    public:
        friend class RayTracing_Scene;

        explicit Scene(Device& device);
        ~Scene();

    public:
        auto& GetPerspectiveProjection() { return m_PerspectiveProjection; }
        const auto& GetPerspectiveProjection() const { return m_PerspectiveProjection; }

        auto& GetCamera() { return m_Camera; }
        const auto& GetCamera() const { return m_Camera; }

        const auto& GetStats() const { return m_Stats; }

        const Descriptor& GetPointLightBufferStructuredSrv() const;

        auto& GetEntityRegistry() { return m_EntityRegistry; }
        const auto& GetEntityRegistry() const { return m_EntityRegistry; }

        const auto& GetMeshRegistry() const { return m_MeshRegistry; }

    public:
        void OnUpdate();

        entt::entity AddMesh(MeshResource&& meshResource);

        void UploadMeshesToGpu();

    private:
        void OnTransformComponentConstuct(entt::registry& registry, entt::entity entityHandle);

        void PushTextures(std::span<const TextureImage> textureImages);

        void UploadAllMeshData();
        void UploadAllMeshInstances();
        void UploadAllTextures();
        void UploadAllMaterials();

        void UpdateStats(entt::entity meshHandle);

    private:
        Device& m_Device;

        SceneStats m_Stats;

        PerspectiveProjection m_PerspectiveProjection;
        Camera m_Camera{ m_PerspectiveProjection };

        std::vector<std::vector<std::byte>> m_TexturesData;
        std::vector<std::unique_ptr<Texture>> m_Textures;

        std::unique_ptr<Buffer> m_PointLightBuffer;

        entt::registry m_EntityRegistry;
        entt::registry m_MeshRegistry;
    };

}
