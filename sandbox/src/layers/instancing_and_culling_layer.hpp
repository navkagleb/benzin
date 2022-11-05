#pragma once

#include <spieler/math/math.hpp>

#include <spieler/core/layer.hpp>

#include <spieler/renderer/buffer.hpp>
#include <spieler/renderer/texture.hpp>
#include <spieler/renderer/context.hpp>
#include <spieler/renderer/mesh.hpp>
#include <spieler/renderer/root_signature.hpp>
#include <spieler/renderer/shader.hpp>
#include <spieler/renderer/pipeline_state.hpp>
#include <spieler/renderer/camera.hpp>

namespace spieler::renderer
{

    class Device;

} // namespace spieler::renderer

namespace sandbox
{

    class DynamicCubeMap
    {
    public:
        DynamicCubeMap(uint32_t width, uint32_t height);

    public:
        spieler::renderer::Texture& GetCubeMap() { return m_CubeMap; }
        spieler::renderer::Texture& GetDepthStencil() { return m_DepthStencil; }

        const DirectX::XMVECTOR& GetPosition() const { return m_Position; }
        void SetPosition(const DirectX::XMVECTOR& position);

        const spieler::renderer::Camera& GetCamera(uint32_t index) const { return m_Cameras[index]; }

        const spieler::renderer::Viewport& GetViewport() const { return m_Viewport; }
        const spieler::renderer::ScissorRect& GetScissorRect() const { return m_ScissorRect; }

    public:
        void OnResize(uint32_t width, uint32_t height);

    private:
        void InitCubeMap(spieler::renderer::Device& device, uint32_t width, uint32_t height);
        void InitDepthStencil(spieler::renderer::Device& device, uint32_t width, uint32_t height);
        void InitCameras(uint32_t width, uint32_t height);
        void InitViewport(float width, float height);

    private:
        spieler::renderer::Texture m_CubeMap;
        spieler::renderer::Texture m_DepthStencil;

        DirectX::XMVECTOR m_Position{ 0.0f, 0.0f, 0.0f, 1.0f };
        std::array<spieler::renderer::Camera, 6> m_Cameras;

        spieler::renderer::Viewport m_Viewport;
        spieler::renderer::ScissorRect m_ScissorRect;
    };

    class InstancingAndCullingLayer final : public spieler::Layer
    {
    public:
        struct RenderItem
        {
            struct Instance
            {
                spieler::renderer::Transform Transform;
                uint32_t MaterialIndex{ 0 };
                bool Visible{ true };
            };

            const spieler::renderer::MeshGeometry* MeshGeometry{ nullptr };
            const spieler::renderer::SubmeshGeometry* SubmeshGeometry{ nullptr };
            spieler::renderer::PrimitiveTopology PrimitiveTopology{ spieler::renderer::PrimitiveTopology::Unknown };

            bool IsNeedCulling{ true };

            std::vector<Instance> Instances;
            uint32_t VisibleInstanceCount{ 0 };
            uint32_t StructuredBufferOffset{ 0 };
        };

        struct PickedRenderItem
        {
            RenderItem* RenderItem{ nullptr };

            uint32_t InstanceIndex{ 0 };
            uint32_t TriangleIndex{ 0 };
        };

        struct LightData
        {
            DirectX::XMFLOAT3 Strength{ 1.0f, 1.0f, 1.0f };
            float FalloffStart{ 0.0f };
            DirectX::XMFLOAT3 Direction{ 0.0f, 0.0f, 0.0f };
            float FalloffEnd{ 0.0f };
            DirectX::XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
            float SpotPower{ 0.0f };
        };

        struct PassData
        {
            DirectX::XMFLOAT4 AmbientLight{ 0.1f, 0.1f, 0.1f, 1.0f };
            std::vector<LightData> Lights;
        };

        struct MaterialData
        {
            DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 FresnelR0{ 0.01f, 0.01f, 0.01f };
            float Roughness{ 0.25f };
            DirectX::XMMATRIX Transform{ DirectX::XMMatrixIdentity() };
            uint32_t DiffuseMapIndex{ 0 };
            uint32_t StructuredBufferIndex{ 0 };
        };

        class PointLightController
        {
        public:
            PointLightController() = default;
            PointLightController(LightData* light);

        public:
            void OnImGuiRender();

        private:
            InstancingAndCullingLayer::LightData* m_Light{ nullptr };
        };

    public:
        bool OnAttach() override;

        void OnEvent(spieler::Event& event) override;
        void OnUpdate(float dt) override;
        void OnRender(float dt) override;
        void OnImGuiRender(float dt) override;

    private:
        void InitTextures();
        void InitMeshGeometries();

        void InitBuffers();

        void InitRootSignature();
        void InitPipelineState();

        void InitViewport();
        void InitScissorRect();
        void InitDepthStencil();

        void OnWindowResized();

        void PickTriangle(float x, float y);

        void RenderRenderItems(const spieler::renderer::PipelineState& pso, const std::span<RenderItem*>& renderItems) const;

    private:
        static const spieler::renderer::GraphicsFormat ms_DepthStencilFormat{ spieler::renderer::GraphicsFormat::D24UnsignedNormS8UnsignedInt };

    private:
        spieler::renderer::Viewport m_Viewport;
        spieler::renderer::ScissorRect m_ScissorRect;

        std::unordered_map<std::string, spieler::renderer::Buffer> m_Buffers;
        std::unordered_map<std::string, spieler::renderer::Texture> m_Textures;

        spieler::renderer::MeshGeometry m_MeshGeometry;
        spieler::renderer::RootSignature m_RootSignature;
        spieler::renderer::ShaderLibrary m_ShaderLibrary;
        std::unordered_map<std::string, spieler::renderer::PipelineState> m_PipelineStates;
        
        std::unordered_map<std::string, std::unique_ptr<RenderItem>> m_RenderItems;
        std::vector<RenderItem*> m_DefaultRenderItems;
        std::vector<RenderItem*> m_LightRenderItems;
        RenderItem* m_DynamicCubeRenderItem{ nullptr };
        RenderItem* m_CubeMapRenderItem{ nullptr };
        PickedRenderItem m_PickedRenderItem;

        PassData m_PassData;
        std::unordered_map<std::string, MaterialData> m_Materials;

        spieler::renderer::Camera m_Camera;
        spieler::renderer::CameraController m_CameraController{ m_Camera };

        PointLightController m_PointLightController;

        std::unique_ptr<DynamicCubeMap> m_DynamicCubeMap;

        bool m_IsCullingEnabled{ true };
    };

} // namespace sandbox
