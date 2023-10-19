#pragma once

#include <benzin/core/layer.hpp>
#include <benzin/engine/camera.hpp>
#include <benzin/engine/entity.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/system/window.hpp>

namespace sandbox
{

    class RaytracingLayer : public benzin::Layer
    {
    private:
        struct RayGenConstants
        {
            struct ViewportRect
            {
                float Left = 0.0f;
                float Top = 0.0f;
                float Right = 0.0f;
                float Bottom = 0.0f;
            };

            ViewportRect Viewport;
            ViewportRect Stencil;
        };

    public:
        explicit RaytracingLayer(const benzin::GraphicsRefs& graphicsRefs);

    public:
        void OnEvent(benzin::Event& event) override;
        void OnUpdate(float dt) override;
        void OnRender() override;
        void OnImGuiRender() override;

    private:
        void CreateEntities();

        void CreateRootSignatures();
        void CreateRaytracingStateObject();
        void CreateAccelerationStructures();
        void CreateShaderTables();
        void CreateRaytracingResources();

    private:
        benzin::Window& m_Window;
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        entt::registry m_EntityRegistry;
        entt::entity m_BoxEntity{ 0 }; // Use brackets because entt::entity is enumm

        std::vector<DirectX::XMFLOAT3> m_Vertices;
        std::vector<uint32_t> m_Indices;

        std::shared_ptr<benzin::Buffer> m_VertexBuffer;
        std::shared_ptr<benzin::Buffer> m_IndexBuffer;

        // Raytracing stuff
        ComPtr<ID3D12RootSignature> m_D3D12LocalRootSignature;

        ComPtr<ID3D12StateObject> m_D3D12RaytracingStateObject;

        std::shared_ptr<benzin::Buffer> m_TLAS;
        std::shared_ptr<benzin::Buffer> m_BLAS;

        std::shared_ptr<benzin::Buffer> m_RayGenShaderTable;
        std::shared_ptr<benzin::Buffer> m_MissShaderTable;
        std::shared_ptr<benzin::Buffer> m_HitGroupShaderTable;

        std::shared_ptr<benzin::Texture> m_RaytracingOutput;

        RayGenConstants m_RayGenConstants
        {
            .Viewport{ -1.0f, -1.0f, 1.0f, 1.0f },
            .Stencil
            {
                -1.0f + 0.1f / m_SwapChain.GetAspectRatio(), -1.0f + 0.1f,
                1.0f - 0.1f / m_SwapChain.GetAspectRatio(), 1.0f - 0.1f,
            },
        };
    };

} // namespace sandbox
