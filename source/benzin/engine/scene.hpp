#pragma once

#include <benzin/engine/camera.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/mesh.hpp>

#include <shaders/joint/procedural_grass_resources.hpp>

namespace joint
{
    struct MeshDraw;
}

namespace benzin
{

    class Buffer;
    class Device;
    class Texture;

    struct MeshRange
    {
        uint32_t m_DrawPartOffset = 0;
        uint32_t m_DrawPartCount = 0;
    };

    struct MeshDraw
    {
        DirectX::XMFLOAT3 m_Translation = {};
        DirectX::XMFLOAT3 m_Rotation = {};
        float m_Scale = 1.0f;

        uint32_t m_MeshRangeIndex = g_MaxU32;
    };

    class Scene
    {
    public:
        using UpdateCallback = std::function<void()>;

        explicit Scene(Device& device);
        ~Scene();

        void AddMesh(
            const std::string& debugName,
            Mesh&& mesh,
            std::vector<MeshDrawPart>&& meshDrawParts,
            std::vector<Material>&& materials = {},
            std::vector<TextureImage>&& textures = {});

        void UploadToGpu();
        void UploadMeshletsToGpu();

        void ExecuteUpdateCallbacks();
        void UploadMeshDrawsToGpu();

        Device& m_Device;

        PerspectiveCamera m_Camera;

        std::vector<joint::MeshVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        std::vector<MeshPart> m_MeshParts;
        std::vector<MeshDrawPart> m_MeshDrawParts;
        std::vector<Material> m_Materials;
        std::vector<std::vector<std::byte>> m_TexturesData;

        std::unordered_map<std::string, uint32_t> m_MeshRangeMap;
        std::vector<MeshRange> m_MeshRanges;

        std::vector<MeshDraw> m_MeshDraws;
        std::vector<joint::MeshDraw> m_JointMeshDraws;

        std::unique_ptr<Buffer> m_VertexBuffer;
        std::unique_ptr<Buffer> m_IndexBuffer;
        std::unique_ptr<Buffer> m_MeshDrawPartBuffer;
        std::unique_ptr<Buffer> m_MeshDrawBuffer;
        std::unique_ptr<Buffer> m_MaterialBuffer;
        std::vector<std::unique_ptr<Texture>> m_Textures;





        std::vector<joint::GrassPatch> m_GrassPatches;
        SunLight m_SunLight;

        std::vector<UpdateCallback> m_UpdateCallbacks;
    };

}
