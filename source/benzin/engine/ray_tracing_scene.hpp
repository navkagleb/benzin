#pragma once

namespace benzin
{

    class Buffer;
    class Device;
    class Scene;
    class TopLevelAccelerationStructure;

    class RayTracingScene
    {
    public:
        RayTracingScene(Device& device, Scene& scene);
        ~RayTracingScene();

        void BuildBlases();
        [[nodiscard]] uint64_t BuildTlas();

    private:
        void ProcessMeshes();
        void UploadLocalTransformsToGpu(std::unique_ptr<benzin::Buffer>& tempLocalTransformBuffer);
        void CreateBlases(const benzin::Buffer& tempLocalTransformBuffer);

    private:
        Device& m_Device;
        Scene& m_Scene;

        std::vector<std::unique_ptr<TopLevelAccelerationStructure>> m_Tlases;
    };

}
