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

    enum RtShadowRc : uint32_t
    {
        RtShadowRc_PassConstantBuffer = GlobalRc_Count,
        RtShadowRc_GBufferWorldNormalTexture,
        RtShadowRc_GBufferDepthTexture,
        RtShadowRc_PointLightBuffer,
        RtShadowRc_VisiblityBuffer,
        RtShadowRc_Count,
    };

    enum DenoiserTemporalAccumulationRc : uint32_t
    {
        DenoiserTemporalAccumulationRc_WorldNormalTexture = GlobalRc_Count,
        DenoiserTemporalAccumulationRc_VelocityBuffer,
        DenoiserTemporalAccumulationRc_DepthBuffer,
        DenoiserTemporalAccumulationRc_PreviousViewDepthBuffer,
        DenoiserTemporalAccumulationRc_PreviousTemporalAccumulationBuffer,
        DenoiserTemporalAccumulationRc_CurrentTemporalAccumulationBuffer,
        DenoiserTemporalAccumulationRc_Count,
    };

    enum MipGenerationRc : uint32_t
    {
        MipGenerationRc_PassConstantBuffer = GlobalRc_Count,
        MipGenerationRc_SourceMip,
        MipGenerationRc_DestinationMip0,
        MipGenerationRc_DestinationMip1,
        MipGenerationRc_DestinationMip2,
        MipGenerationRc_DestinationMip3,
        MipGenerationRc_Count,
    };

    enum DenoiserHistoryFixRc : uint32_t
    {
        DenoiserHistoryFixRc_GBufferAlbedoAndRoughness = GlobalRc_Count,
        DenoiserHistoryFixRc_TemporalAccumulationBuffer,
        DenoiserHistoryFixRc_ViewDepthBuffer,
        DenoiserHistoryFixRc_NoisyVisibilityBuffer,
        DenoiserHistoryFixRc_DenoisedVisibilityBuffer,
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
        FullScreenDebugRc_ViewDepthBuffer,
        FullScreenDebugRc_DepthBuffer,
        FullScreenDebugRc_ShadowVisibilityBuffer,
        FullScreenDebugRc_TemporalAccumulationBuffer,
        FullScreenDebugRc_Count,
    };

    enum EquirectangularToCubePassRc : uint32_t
    {
        EquirectangularToCubeRc_EquirectangularTexture = GlobalRc_Count,
        EquirectangularToCubeRc_OutCubeTexture,
        EquirectangularToCubeRc_Count,
    };

} // namespace joint
