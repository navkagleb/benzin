#pragma once

#include <benzin/graphics/ray_tracing_acceleration_structures.hpp>

namespace benzin
{

    class Buffer;
    class Device;
    class RayTracing_Tlas;
    class Scene;

    class RayTracing_Scene
    {
    public:
        RayTracing_Scene(Device& device, Scene& scene);
        ~RayTracing_Scene();

        const RayTracing_Tlas& GetActiveTlas() const;

        void BuildBlases();
        void UpdateTlasBuffers();

    private:
        void ProcessMeshes(std::unique_ptr<benzin::Buffer>& localTransformBuffer);
        void CreateBlases();

    private:
        Device& m_Device;
        Scene& m_Scene;

        RayTracing_Tlas m_Tlases[BENZIN_FRAME_COUNT] = {};
    };

}
