#pragma once

#include <benzin/engine/camera.hpp>
#include <benzin/engine/mesh.hpp>

namespace joint
{
    struct GrassPatch;
    struct MeshDraw;
}

namespace benzin
{

    class Buffer;
    class Device;
    class Texture;

    struct MeshGeometryDraw
    {
        DirectX::XMFLOAT3 m_Translation = {};
        DirectX::XMFLOAT3 m_Rotation = {};
        float m_Scale = 1.0f;

        uint32_t m_MeshDrawOffset = 0;
        uint32_t m_MeshDrawCount = 0;
    };

    struct SunLight
    {
        DirectX::XMFLOAT3 m_Color = { 1.0f, 1.0f, 1.0f };
        float m_Intensity = 1.0f;

        float m_AngularDiameterInRadians = DirectX::XMConvertToRadians(0.5f); // [0.01f, 5.0f]
        float m_AzimuthInRadians = DirectX::XMConvertToRadians(0.0f); // [-180.0f, 180.0f]
        float m_ElevationInRadians = DirectX::XMConvertToRadians(45.0f); // [0.0f, 180.0]
    };

    struct Scene
    {
        PerspectiveCamera m_Camera;

        std::vector<joint::GrassPatch> m_GrassPatches;
        SunLight m_SunLight;

        MeshGeometry m_Geometry;
        std::vector<MeshDraw> m_MeshDraws;
        std::vector<Material> m_Materials;
        std::vector<TextureImage> m_TextureImages;

        std::vector<MeshGeometryDraw> m_MeshGeometryDraws;
        std::vector<joint::MeshDraw> m_JointMeshDraws;

        std::unique_ptr<Buffer> m_VertexBuffer;
        std::unique_ptr<Buffer> m_IndexBuffer;
        std::unique_ptr<Buffer> m_MeshBuffer;
        std::unique_ptr<Buffer> m_MeshletBuffer;
        std::unique_ptr<Buffer> m_MeshletCullVolumeBuffer;
        std::unique_ptr<Buffer> m_MeshletVertexIndexBuffer;
        std::unique_ptr<Buffer> m_MeshletIndexBuffer;
        std::unique_ptr<Buffer> m_MaterialBuffer;
        std::vector<std::unique_ptr<Texture>> m_Textures;

        std::unique_ptr<Buffer> m_MeshDrawBuffer;

        Scene();
        ~Scene();

        struct MeshGeometryRange
        {
            uint32_t m_MeshDrawOffset = 0;
            uint32_t m_MeshDrawCount = 0;
        };

        MeshGeometryRange AddMeshGeometry(
            const std::string& debugName,
            MeshGeometry&& geometry,
            std::vector<MeshDraw>&& meshDraws,
            std::vector<Material>&& materials = {},
            std::vector<TextureImage>&& textures = {});

        void UploadMeshGeometryToGpu(Device& device);
        void UploadMeshDrawsToGpu(Device& device);
    };

}
