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
        auto& GetCamera(this auto&& self) { return self.m_Camera; }

        auto& GetEntityRegistry(this auto&& self) { return self.m_EntityRegistry; }
        const auto& GetMeshRegistry() const { return m_MeshRegistry; }

        const auto& GetUnifiedMaterialBuffer() const { return *m_UnifiedMaterialBuffer; }
        const auto& GetLightBuffer() const { return *m_LightBuffer; }

        auto GetActiveLightCount() const { return m_ActiveLightCount; }
        
        auto GetSunEntity() const { return m_SunEntity; }
        auto GetUnitSphereMeshHandle() const { return m_UnitSphereMeshHandle; }

        const Material& GetMaterial(uint32_t index) const;

    public:
        entt::entity AddMesh(MeshResource&& meshResource);

        void UploadMeshesToGpu();
        void UploadMeshletsToGpu();
        void UploadMaterialsToGpu();

        void UpdateEntities();
        void UploadLightsToGpu();

        void EndFrame();

    private:
        uint32_t AddTextures(std::span<TextureImage> textureImages);
        uint32_t AddMaterials(std::span<TextureImage> textureImages, std::span<const MeshResource::Material> materials);
        uint32_t GetTextureGpuHeapIndex(uint32_t textureOffset, uint32_t localTextureIndex) const;

        void UploadPixelDataSetToGpu();

    private:
        Device& m_Device;

        PerspectiveCamera m_Camera;

        entt::registry m_EntityRegistry;
        entt::registry m_MeshRegistry;

        std::vector<std::vector<std::byte>> m_PixelDataSet;
        std::vector<std::unique_ptr<Texture>> m_Textures;

        std::vector<Material> m_UnifiedMaterials;
        std::unique_ptr<Buffer> m_UnifiedMaterialBuffer;

        std::unique_ptr<Buffer> m_LightBuffer;
        uint32_t m_ActiveLightCount = 0;

        entt::entity m_SunEntity;
        entt::entity m_UnitSphereMeshHandle;
    };

}
