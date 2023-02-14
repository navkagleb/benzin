#pragma once

#if 0

#include <benzin/core/layer.hpp>
#include <benzin/system/window.hpp>

#include <benzin/graphics/root_signature.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/shader.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/graphics_command_list.hpp>

#include <benzin/engine/camera.hpp>
#include <benzin/engine/mesh.hpp>

namespace sandbox
{

    class TessellationLayer final : public benzin::Layer
    {
    private:
        struct PassConstants
        {
            DirectX::XMMATRIX View{};
            DirectX::XMMATRIX Projection{};
            DirectX::XMMATRIX ViewProjection{};
            DirectX::XMFLOAT3 CameraPosition{};
        };

        struct ObjectConstants
        {
            DirectX::XMMATRIX World{ DirectX::XMMatrixIdentity() };
        };

    public:
        bool OnAttach() override;

        void OnEvent(benzin::Event& event) override;
        void OnUpdate(float dt) override;
        void OnRender(float dt) override;

    private:
        void InitViewport();
        void InitScissorRect();
        void InitDepthStencil();

        bool OnWindowResized(benzin::WindowResizedEvent& event);

    private:
        benzin::Viewport m_Viewport;
        benzin::ScissorRect m_ScissorRect;

        benzin::MeshGeometry m_MeshGeometry;
        benzin::RootSignature m_RootSignature;
        std::unordered_map<std::string, std::shared_ptr<benzin::Shader>> m_ShaderLibrary;
        std::unordered_map<std::string, benzin::PipelineState> m_PipelineStates;
        std::unordered_map<std::string, benzin::RenderItem> m_RenderItems;

        PassConstants m_PassConstants;
        std::shared_ptr<benzin::BufferResource> m_PassConstantBuffer;

        std::shared_ptr<benzin::BufferResource> m_ObjectConstantBuffer;

        benzin::GraphicsFormat m_DepthStencilFormat{ benzin::GraphicsFormat::D24UnsignedNormS8UnsignedInt };
        benzin::Texture m_DepthStencil;

        benzin::Camera m_Camera;
        benzin::CameraController m_CameraController{ m_Camera };
    };

} // namespace sandbox

#endif
