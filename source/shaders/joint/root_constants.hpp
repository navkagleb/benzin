#pragma once

namespace joint
{

    enum GlobalRc : uint32_t
    {
        GlobalRc_FrameConstantBuffer,
        GlobalRc_CameraConstantBuffer,
        GlobalRc_Count,
    };

    enum GeometryPassRc : uint32_t
    {
        GeometryPassRc_MeshVertexBuffer = GlobalRc_Count,
        GeometryPassRc_MeshIndexBuffer,
        GeometryPassRc_MeshInfoBuffer,
        GeometryPassRc_MeshInstanceBuffer,
        GeometryPassRc_MaterialBuffer,
        GeometryPassRc_MeshTransformConstantBuffer,
        GeometryPassRc_MeshInstanceIndex,
        GeometryPassRc_Count,
    };

    enum RtShadowPassRc : uint32_t
    {
        RtShadowPassRc_PassConstantBuffer = GlobalRc_Count,
        RtShadowPassRc_GBufferWorldNormalTexture,
        RtShadowPassRc_GBufferDepthTexture,
        RtShadowPassRc_PointLightBuffer,
        RtShadowPassRc_VisiblityBuffer,
        RtShadowPassRc_Count,
    };

    enum RtShadowDenoisingRc : uint32_t
    {
        RtShadowDenoisingRc_PassConstantBuffer = GlobalRc_Count,
        RtShadowDenoisingRc_WorldNormalTexture,
        RtShadowDenoisingRc_VelocityBuffer,
        RtShadowDenoisingRc_DepthBuffer,
        RtShadowDenoisingRc_PreviousViewDepthBuffer,
        RtShadowDenoisingRc_ShadowVisibilityBuffer,
        RtShadowDenoisingRc_PreviousTemporalAccumulationBuffer,
        RtShadowDenoisingRc_CurrentTemporalAccumulationBuffer,
        RtShadowDenoisingRc_Count,
    };

    enum DeferredLightingPassRc : uint32_t
    {
        DeferredLightingPassRc_PassConstantBuffer = GlobalRc_Count,
        DeferredLightingPassRc_AlbedoAndRoughnessTexture,
        DeferredLightingPassRc_EmissiveAndMetallicTexture,
        DeferredLightingPassRc_WorldNormalTexture,
        DeferredLightingPassRc_VelocityBuffer,
        DeferredLightingPassRc_DepthStencilTexture,
        DeferredLightingPassRc_PointLightBuffer,
        DeferredLightingPassRc_ShadowVisibilityBuffer,
        DeferredLightingPassRc_Count,
    };

    enum EnvironmentPassRc : uint32_t
    {
        EnvironmentPassRc_CubeMapTexture = GlobalRc_Count,
        EnvironmentPassRc_Count,
    };

    enum FullScreenDebugRc : uint32_t
    {
        FullScreenDebugRc_PassConstantBuffer = GlobalRc_Count,
        FullScreenDebugRc_AlbedoAndRoughnessTexture,
        FullScreenDebugRc_EmissiveAndMetallicTexture,
        FullScreenDebugRc_WorldNormalTexture,
        FullScreenDebugRc_VelocityBuffer,
        FullScreenDebugRc_DepthBuffer,
        FullScreenDebugRc_ShadowVisiblityBuffer,
        FullScreenDebugRc_TemporalAccumulationBuffer,
        FullScreenDebugRc_Count,
    };

    enum EquirectangularToCubePassRc : uint32_t
    {
        EquirectangularToCubePassRc_EquirectangularTexture = GlobalRc_Count,
        EquirectangularToCubePassRc_OutCubeTexture,
        EquirectangularToCubePassRc_Count,
    };

} // namespace joint
