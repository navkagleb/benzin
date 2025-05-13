#pragma once

#include <benzin/engine/camera.hpp>
#include <benzin/engine/mesh.hpp>

namespace benzin
{

    class Buffer;
    class Descriptor;
    class Device;
    class Texture;

    struct Material
    {
        MaterialTextureIndices TextureGpuHeapIndices;
        MaterialConsts Consts;
    };

    using MeshTag = std::string;

    class Scene
    {
    public:
        friend class RayTracing_Scene;
        friend class SceneStatsTool;

        static inline const uint32_t s_MaxLightCount = 4; // TODO: Sun + 3 Spherical Lights

        explicit Scene(Device& device);
        ~Scene();

    public:
        auto& GetPerspectiveProjection(this auto&& self) { return self.m_PerspectiveProjection; }
        auto& GetCamera(this auto&& self) { return self.m_Camera; }

        const Descriptor& GetEntityTransformBufferSrv() const;
        const Descriptor& GetUnifiedMaterialBufferSrv() const;

        auto& GetEntityRegistry(this auto&& self) { return self.m_EntityRegistry; }
        const auto& GetMeshRegistry() const { return m_MeshRegistry; }

        auto GetSunEntity() const { return m_SunEntity; }

        uint64_t GetLightBufferGpuAddress() const;
        uint32_t GetActiveLightCount() const { return m_ActiveLightCount; }

        const Material& GetMaterial(uint32_t index) const;

    public:
        entt::entity AddMesh(MeshResource&& meshResource);

        void UploadMeshesToGpu();
        void UploadMeshletsToGpu();
        void UploadMaterialsToGpu();

        void UpdateEntities();
        void UploadEntityTransformsToGpu();
        void UploadLightsToGpu();

    private:
        uint32_t AddTextures(std::span<TextureImage> textureImages);
        uint32_t AddMaterials(std::span<TextureImage> textureImages, std::span<const MeshResource::Material> materials);
        uint32_t GetTextureGpuHeapIndex(uint32_t textureOffset, uint32_t localTextureIndex) const;

        void UploadPixelDataSetToGpu();

    private:
        Device& m_Device;

        PerspectiveProjection m_PerspectiveProjection;
        Camera m_Camera{ m_PerspectiveProjection };

        std::vector<std::vector<std::byte>> m_PixelDataSet;
        std::vector<std::unique_ptr<Texture>> m_Textures;

        std::vector<Material> m_UnifiedMaterials;
        std::unique_ptr<Buffer> m_UnifiedMaterialBuffer;

        entt::entity m_SunEntity;
        std::vector<entt::entity> m_SphericalLightEntities;
        std::unique_ptr<Buffer> m_LightBuffer;
        uint32_t m_ActiveLightCount = 0;

        uint32_t m_EntityTransformCount = 0;
        std::unique_ptr<Buffer> m_EntityTransformBuffer;

        entt::registry m_EntityRegistry;
        entt::registry m_MeshRegistry;
    };

}
