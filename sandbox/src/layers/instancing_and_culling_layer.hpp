#pragma once

#include <spieler/math/math.hpp>

#include <spieler/core/layer.hpp>

#include <spieler/system/window.hpp>
#include <spieler/graphics/buffer.hpp>
#include <spieler/graphics/texture.hpp>
#include <spieler/graphics/root_signature.hpp>
#include <spieler/graphics/shader.hpp>
#include <spieler/graphics/pipeline_state.hpp>
#include <spieler/graphics/graphics_command_list.hpp>
#include <spieler/graphics/command_queue.hpp>
#include <spieler/graphics/swap_chain.hpp>
#include <spieler/engine/mesh.hpp>
#include <spieler/engine/camera.hpp>

namespace sandbox
{

    class DynamicCubeMap
    {
    public:
        DynamicCubeMap(spieler::Device& device, uint32_t width, uint32_t height);

    public:
        spieler::Texture& GetCubeMap() { return m_CubeMap; }
        spieler::Texture& GetDepthStencil() { return m_DepthStencil; }

        const DirectX::XMVECTOR& GetPosition() const { return m_Position; }
        void SetPosition(const DirectX::XMVECTOR& position);

        const spieler::Camera& GetCamera(uint32_t index) const { return m_Cameras[index]; }

        const spieler::Viewport& GetViewport() const { return m_Viewport; }
        const spieler::ScissorRect& GetScissorRect() const { return m_ScissorRect; }

    public:
        void OnResize(spieler::Device& device, uint32_t width, uint32_t height);

    private:
        void InitCubeMap(spieler::Device& device, uint32_t width, uint32_t height);
        void InitDepthStencil(spieler::Device& device, uint32_t width, uint32_t height);
        void InitCameras(uint32_t width, uint32_t height);
        void InitViewport(float width, float height);

    private:
        spieler::Texture m_CubeMap;
        spieler::Texture m_DepthStencil;

        DirectX::XMVECTOR m_Position{ 0.0f, 0.0f, 0.0f, 1.0f };
        std::array<spieler::Camera, 6> m_Cameras;

        spieler::Viewport m_Viewport;
        spieler::ScissorRect m_ScissorRect;
    };

    class InstancingAndCullingLayer final : public spieler::Layer
    {
    public:
        struct RenderItem
        {
            struct Instance
            {
                spieler::Transform Transform;
                uint32_t MaterialIndex{ 0 };
                bool Visible{ true };
            };

            const spieler::MeshGeometry* MeshGeometry{ nullptr };
            const spieler::SubmeshGeometry* SubmeshGeometry{ nullptr };
            spieler::PrimitiveTopology PrimitiveTopology{ spieler::PrimitiveTopology::Unknown };

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
        InstancingAndCullingLayer(spieler::Window& window, spieler::Device& device, spieler::CommandQueue& commandQueue, spieler::SwapChain& swapChain);

    public:
        bool OnAttach() override;
        bool OnDetach() override;

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

        void InitRenderTarget();
        void InitDepthStencil();

        void OnWindowResized();

        void PickTriangle(float x, float y);

        void RenderRenderItems(const spieler::PipelineState& pso, const std::span<RenderItem*>& renderItems);
        void RenderFullscreenQuad(const spieler::TextureShaderResourceView& srv);

    private:
        static const spieler::GraphicsFormat ms_DepthStencilFormat{ spieler::GraphicsFormat::D24UnsignedNormS8UnsignedInt };

    private:
        spieler::Window& m_Window;
        spieler::Device& m_Device;
        spieler::CommandQueue& m_CommandQueue;
        spieler::SwapChain& m_SwapChain;

        spieler::GraphicsCommandList m_GraphicsCommandList;

        std::unordered_map<std::string, spieler::Buffer> m_Buffers;
        std::unordered_map<std::string, spieler::Texture> m_Textures;

        spieler::MeshGeometry m_MeshGeometry;
        spieler::RootSignature m_RootSignature;
        std::unordered_map<std::string, std::shared_ptr<spieler::Shader>> m_ShaderLibrary;
        std::unordered_map<std::string, spieler::PipelineState> m_PipelineStates;
        
        std::unordered_map<std::string, std::unique_ptr<RenderItem>> m_RenderItems;
        std::vector<RenderItem*> m_DefaultRenderItems;
        std::vector<RenderItem*> m_LightRenderItems;
        RenderItem* m_DynamicCubeRenderItem{ nullptr };
        RenderItem* m_CubeMapRenderItem{ nullptr };
        PickedRenderItem m_PickedRenderItem;

        PassData m_PassData;
        std::unordered_map<std::string, MaterialData> m_Materials;

        spieler::Camera m_Camera;
        spieler::CameraController m_CameraController{ m_Camera };

        PointLightController m_PointLightController;

        std::unique_ptr<DynamicCubeMap> m_DynamicCubeMap;

        bool m_IsCullingEnabled{ true };
    };

} // namespace sandbox
