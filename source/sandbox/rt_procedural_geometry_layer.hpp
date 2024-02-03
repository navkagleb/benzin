#pragma once

#include <benzin/core/layer.hpp>
#include <benzin/engine/camera.hpp>
#include <benzin/graphics/swap_chain.hpp>

namespace benzin
{

    class Buffer;
    class Device;
    class GPUTimer;
    class Texture;
    class Window;

    struct GraphicsRefs;

    namespace rt
    {

        class BottomLevelAccelerationStructure;
        class TopLevelAccelerationStructure;

    } // namespace rt

} // namespace benzin

namespace sandbox
{

    class RTProceduralGeometryLayer : public benzin::Layer
    {
    public:
        enum class GeometryType : uint8_t
        {
            Triangle,
            AABB,
        };

    public:
        explicit RTProceduralGeometryLayer(const benzin::GraphicsRefs& graphicsRefs);
        ~RTProceduralGeometryLayer();

    public:
        void OnEndFrame() override;
        
        void OnEvent(benzin::Event& event) override;
        void OnUpdate() override;
        void OnRender() override;
        void OnImGuiRender() override;

    private:
        void CreateGeometry();
        void CreatePipelineStateObject();
        void CreateAccelerationStructures();
        void CreateShaderTables();
        void CreateRaytracingResources();

    private:
        benzin::Window& m_Window;
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::GPUTimer> m_GPUTimer;

        std::unique_ptr<benzin::Buffer> m_GridVertexBuffer;
        std::unique_ptr<benzin::Buffer> m_GridIndexBuffer;
        std::unique_ptr<benzin::Buffer> m_MeshMaterialBuffer;

        std::unique_ptr<benzin::Buffer> m_AABBBuffer;
        std::unique_ptr<benzin::Buffer> m_ProceduralGeometryBuffer;

        ComPtr<ID3D12StateObject> m_D3D12RaytracingStateObject;

        magic_enum::containers::array<GeometryType, std::unique_ptr<benzin::rt::BottomLevelAccelerationStructure>> m_BottomLevelASs;
        std::unique_ptr<benzin::rt::TopLevelAccelerationStructure> m_TopLevelAS;

        std::unique_ptr<benzin::Buffer> m_RayGenShaderTable;
        std::unique_ptr<benzin::Buffer> m_MissShaderTable;
        std::unique_ptr<benzin::Buffer> m_HitGroupShaderTable;

        benzin::PerspectiveProjection m_PerspectiveProjection;
        benzin::Camera m_Camera{ m_PerspectiveProjection };
        benzin::FlyCameraController m_FlyCameraController{ m_Camera };

        std::unique_ptr<benzin::Buffer> m_SceneConstantBuffer;
        std::unique_ptr<benzin::Texture> m_RaytracingOutput;
    };

} // namespace sandbox
