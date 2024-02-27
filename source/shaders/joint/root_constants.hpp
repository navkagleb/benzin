#pragma once

namespace joint
{

    enum GlobalRC : uint32_t
    {
        GlobalRC_CameraConstantBuffer,
        GlobalRC_Count,
    };

    enum GeometryPassRC : uint32_t
    {
        GeometryPassRC_MeshVertexBuffer = GlobalRC_Count,
        GeometryPassRC_MeshIndexBuffer,
        GeometryPassRC_MeshInfoBuffer,
        GeometryPassRC_MeshParentTransformBuffer,
        GeometryPassRC_MeshInstanceBuffer,
        GeometryPassRC_MaterialBuffer,
        GeometryPassRC_MeshTransformConstantBuffer,
        GeometryPassRC_MeshInstanceIndex,
        GeometryPassRC_Count,
    };

    enum RTShadowPassRC : uint32_t
    {
        RTShadowPassRC_PassConstantBuffer = GlobalRC_Count,
        RTShadowPassRC_GBufferWorldNormalTexture,
        RTShadowPassRC_GBufferDepthTexture,
        RTShadowPassRC_PointLightBuffer,
        RTShadowPassRC_NoisyVisiblityBuffer,
        RTShadowPassRC_Count,
    };

    enum RTShadowDenoisingPass : uint32_t
    {
        RTShadowDenoisingPass_PassConstantBuffer = GlobalRC_Count,
        RTShadowDenoisingPass_NoisyVisibilityBuffer,
        RTShadowDenoisingPass_GBufferMotionVectorsTexture,
        RTShadowDenoisingPass_GBufferAlbedoTexture,
        RTShadowDenoisingPass_Count,
    };

    enum DeferredLightingPassRC : uint32_t
    {
        DeferredLightingPassRC_PassConstantBuffer = GlobalRC_Count,
        DeferredLightingPassRC_AlbedoTexture,
        DeferredLightingPassRC_WorldNormalTexture,
        DeferredLightingPassRC_EmissiveTexture,
        DeferredLightingPassRC_RoughnessMetalnessTexture,
        DeferredLightingPassRC_MotionVectorsTexture,
        DeferredLightingPassRC_DepthStencilTexture,
        DeferredLightingPassRC_PointLightBuffer,
        DeferredLightingPassRC_ShadowTexture,
        DeferredLightingPassRC_Count,
    };

    enum EnvironmentPassRC : uint32_t
    {
        EnvironmentPassRC_CubeMapTexture = GlobalRC_Count,
        EnvironmentPassRC_Count,
    };

    enum EquirectangularToCubePassRC : uint32_t
    {
        EquirectangularToCubePassRC_EquirectangularTexture,
        EquirectangularToCubePassRC_OutCubeTexture,
        EquirectangularToCubePassRC_Count,
    };

} // namespace joint
