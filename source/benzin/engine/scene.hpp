#pragma once

#include <benzin/core/math.hpp> // TODO: Remove with SunLight::CalcToSunDirection
#include <benzin/engine/camera.hpp>
#include <benzin/engine/mesh.hpp>

namespace joint
{
    struct GrassPatch;
    struct MeshDraw;
    struct MeshletCullVolume;
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

    struct DrawIndirectCmd
    {
        uint32_t m_DrawIndex = 0;
        D3D12_DRAW_INDEXED_ARGUMENTS m_D3D12Cmd = {};
    };

    struct DispatchMeshIndirectCmd
    {
        uint32_t m_DrawIndex = 0;
        uint32_t m_MeshletOffset = 0;
        uint32_t m_MeshletCount = 0;
        D3D12_DISPATCH_MESH_ARGUMENTS m_D3D12Cmd = {};
    };

    struct SunLight
    {
        DirectX::XMFLOAT3 m_Color = { 1.0f, 1.0f, 1.0f };
        float m_Intensity = 1.0f;

        float m_AngularDiameterInRadians = DirectX::XMConvertToRadians(0.5f); // [0.01f, 5.0f]
        float m_AzimuthInRadians = DirectX::XMConvertToRadians(0.0f); // [-180.0f, 180.0f]
        float m_ElevationInRadians = DirectX::XMConvertToRadians(45.0f); // [0.0f, 180.0]

        // TODO: Move to another place
        DirectX::XMFLOAT3 CalcToSunDirection() const
        {
            const float pitch = m_ElevationInRadians;
            const float yaw = m_AzimuthInRadians;

            auto sunDirection = GetDirectionFromPitchYaw(pitch, yaw); // sunDirection vector directed towards the sun

            DirectX::XMFLOAT3 sunDirection3 = {};
            DirectX::XMStoreFloat3(&sunDirection3, sunDirection);

            return sunDirection3;
        }
    };

    struct Scene
    {
        using UpdateCallback = std::function<void()>;

        Device& m_Device;

        PerspectiveCamera m_Camera;

        std::vector<joint::MeshVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        std::vector<MeshPart> m_MeshParts;
        std::vector<MeshDrawPart> m_MeshDrawParts;

        std::vector<joint::Meshlet> m_Meshlets;
        std::vector<joint::MeshletCullVolume> m_MeshletCullVolumes;
        std::vector<uint32_t> m_MeshletVertexIndices;
        std::vector<uint8_t> m_MeshletIndices;

        std::vector<Material> m_Materials;
        std::vector<std::vector<std::byte>> m_TexturesData;

        std::unordered_map<std::string, uint32_t> m_MeshRangeMap;
        std::vector<MeshRange> m_MeshRanges;

        std::vector<MeshDraw> m_MeshDraws;
        std::vector<joint::MeshDraw> m_JointMeshDraws;

        std::unique_ptr<Buffer> m_VertexBuffer;
        std::unique_ptr<Buffer> m_IndexBuffer;

        std::unique_ptr<Buffer> m_MeshletBuffer;
        std::unique_ptr<Buffer> m_MeshletCullVolumeBuffer;
        std::unique_ptr<Buffer> m_MeshletVertexIndexBuffer;
        std::unique_ptr<Buffer> m_MeshletIndexBuffer;

        std::unique_ptr<Buffer> m_MaterialBuffer;
        std::vector<std::unique_ptr<Texture>> m_Textures;

        std::unique_ptr<Buffer> m_MeshDrawBuffer;
        std::unique_ptr<Buffer> m_MeshDispatchBuffer;
        std::unique_ptr<Buffer> m_DrawIndirectCmdBuffer;

        std::vector<joint::GrassPatch> m_GrassPatches;
        SunLight m_SunLight;

        std::vector<UpdateCallback> m_UpdateCallbacks;

        explicit Scene(Device& device);
        ~Scene();

        void AddMesh(
            const std::string& debugName,
            Mesh&& mesh,
            std::vector<MeshDrawPart>&& meshDrawParts,
            std::vector<Material>&& materials = {},
            std::vector<TextureImage>&& textures = {});

        void UploadToGpu();

        void ExecuteUpdateCallbacks();
        void UploadMeshDrawsToGpu();
    };

}
