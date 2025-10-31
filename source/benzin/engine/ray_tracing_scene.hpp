#pragma once

#include <benzin/graphics/ray_tracing_acceleration_structures.hpp>

namespace benzin
{

    class Buffer;
    class Device;
    struct Scene;

    class RayTracing_Scene
    {
    public:
        RayTracing_Scene(Device& device, Scene& scene);
        ~RayTracing_Scene();

        const auto& GetTlas() const { return m_Tlas; }

        void BuildBlases();
        void UpdateTlasInstances();

    private:
        Device& m_Device;
        Scene& m_Scene;

        RayTracing_Tlas m_Tlas;
        std::vector<RayTracing_Blas> m_Blases;
    };

}
