#pragma once

#include <benzin/core/layer.hpp>
#include <benzin/engine/camera.hpp>

namespace benzin
{

    class Buffer;
    class Device;
    class GPUTimer;
    class PipelineState;
    class SwapChain;
    class Texture;
    class Window;

    struct GraphicsRefs;

} // namespace benzin

namespace sandbox
{

    class RTHelloTriangleLayer : public benzin::Layer
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
        explicit RTHelloTriangleLayer(const benzin::GraphicsRefs& graphicsRefs);

    public:
        void OnEndFrame() override;

        void OnRender() override;
        void OnImGuiRender() override;

    private:
        void CreateEntities();

        void CreateRaytracingStateObject();
        void CreateAccelerationStructures();
        void CreateShaderTables();
        void CreateRaytracingResources();

    private:
        benzin::Window& m_Window;
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::shared_ptr<benzin::GPUTimer> m_GPUTimer;

        std::vector<DirectX::XMFLOAT3> m_Vertices;
        std::vector<uint32_t> m_Indices;

        std::shared_ptr<benzin::Buffer> m_VertexBuffer;
        std::shared_ptr<benzin::Buffer> m_IndexBuffer;

        // RayTracing stuff
        ComPtr<ID3D12StateObject> m_D3D12RaytracingStateObject;

        std::shared_ptr<benzin::Buffer> m_TLAS;
        std::shared_ptr<benzin::Buffer> m_BLAS;

        std::shared_ptr<benzin::Buffer> m_RayGenShaderTable;
        std::shared_ptr<benzin::Buffer> m_MissShaderTable;
        std::shared_ptr<benzin::Buffer> m_HitGroupShaderTable;

        std::shared_ptr<benzin::Buffer> m_RayGenConstantBuffer;
        std::shared_ptr<benzin::Texture> m_RaytracingOutput;

        RayGenConstants m_RayGenConstants;
    };

} // namespace sandbox
