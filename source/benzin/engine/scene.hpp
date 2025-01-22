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

        static inline const uint32_t s_MaxLightCount = 4; // TODO: Sun + 3 Spherical Lights

        Scene(Device& device, TickTimer& animationTimer);
        ~Scene();

    public:
        const auto& GetStats() const { return m_Stats; }

        auto& GetPerspectiveProjection(this auto&& self) { return self.m_PerspectiveProjection; }
        auto& GetCamera(this auto&& self) { return self.m_Camera; }

        Descriptor GetTransformBufferSrv() const;

        auto& GetEntityRegistry(this auto&& self) { return self.m_EntityRegistry; }
        const auto& GetMeshRegistry() const { return m_MeshRegistry; }

        auto GetSunEntity() const { return m_SunEntity; }

        uint64_t GetLightBufferGpuAddress() const;
        uint32_t GetActiveLightCount() const { return m_ActiveLightCount; }

    public:
        void OnUpdate();

        entt::entity AddMesh(MeshResource&& meshResource);

        void UploadMeshesToGpu();

    private:
        void PushTextures(std::span<const TextureImage> textureImages);

        void UploadAllMeshData();
        void UploadAllMeshInstances();
        void UploadAllTextures();
        void UploadAllMaterials();

        void UpdateStats(entt::entity meshHandle);

        void UpdateEntities();
        void UploadTransformsToGpu();
        void UploadLightsToGpu();

    private:
        Device& m_Device;
        TickTimer& m_AnimationTimer;

        SceneStats m_Stats;

        PerspectiveProjection m_PerspectiveProjection;
        Camera m_Camera{ m_PerspectiveProjection };

        std::vector<std::vector<std::byte>> m_TexturesData;
        std::vector<std::unique_ptr<Texture>> m_Textures;

        entt::entity m_SunEntity;
        std::vector<entt::entity> m_SphericalLightEntities;
        std::unique_ptr<Buffer> m_LightBuffer;
        uint32_t m_ActiveLightCount = 0;

        uint32_t m_TransformCount = 0;
        std::unique_ptr<Buffer> m_TransformBuffer;

        entt::registry m_EntityRegistry;
        entt::registry m_MeshRegistry;
    };

}
