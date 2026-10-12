#pragma once

#include <benzin/graphics/ray_tracing_acceleration_structures.hpp>

namespace benzin
{

    class Device;
    struct Scene;

    class RayTracingScene
    {
    public:
        explicit RayTracingScene(const Scene& scene);
        ~RayTracingScene();

        const auto& GetTlas() const { return m_Tlas; }

        void BuildBlases(Device& device);
        void UpdateTlasInstances(Device& device);

    private:
        const Scene& m_Scene;

        RayTracing_Tlas m_Tlas;
        std::vector<RayTracing_Blas> m_Blases;
    };

}
