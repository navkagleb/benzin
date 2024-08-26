#pragma once

namespace joint
{

    enum GeometryPassRc : uint32_t
    {
        GeometryPassRc_MeshVertexBuffer,
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
        RtShadowRc_GBufferWorldNormalTexture,
        RtShadowRc_GBufferDepthTexture,
        RtShadowRc_PointLightBuffer,
        RtShadowRc_VisiblityBuffer,
    };

    enum DenoiserTemporalAccumulationRc : uint32_t
    {
        DenoiserTemporalAccumulationRc_WorldNormalTexture,
        DenoiserTemporalAccumulationRc_VelocityBuffer,
        DenoiserTemporalAccumulationRc_DepthBuffer,
        DenoiserTemporalAccumulationRc_PreviousViewDepthBuffer,
        DenoiserTemporalAccumulationRc_PreviousTemporalAccumulationBuffer,
        DenoiserTemporalAccumulationRc_PreviousDenoisedVisibilityBuffer,
        DenoiserTemporalAccumulationRc_TemporalAccumulationBuffer,
        DenoiserTemporalAccumulationRc_ReprojectedHistoryTexture,
    };

    enum MipGenerationRc : uint32_t
    {
        MipGenerationRc_SourceMip,
        MipGenerationRc_DestinationMip0,
        MipGenerationRc_DestinationMip1,
        MipGenerationRc_DestinationMip2,
        MipGenerationRc_DestinationMip3,
    };

    enum DenoiserHistoryFixRc : uint32_t
    {
        DenoiserHistoryFixRc_GBufferAlbedoAndRoughness,
        DenoiserHistoryFixRc_TemporalAccumulationBuffer,
        DenoiserHistoryFixRc_ViewDepthBuffer,
        DenoiserHistoryFixRc_NoisyVisibilityBuffer,
        DenoiserHistoryFixRc_ReprojectedHistoryTexture,
    };

    enum DenoiserBlurRc : uint32_t
    {
        DenoiserBlurRc_AlbedoAndRoughnessTexture,
        DenoiserBlurRc_WorldNormalTexture,
        DenoiserBlurRc_DepthBuffer,
        DenoiserBlurRc_VelocityTexture,
        DenoiserBlurRc_NoisyVisibilityBuffer,
        DenoiserBlurRc_ReprojectedHistoryTexture,
        DenoiserBlurRc_TemporalAccumulationBuffer,
        DenoiserBlurRc_DenoisedVisibilityBuffer,
    };

    enum DeferredLightingPassRc : uint32_t
    {
        DeferredLightingPassRc_AlbedoAndRoughnessTexture,
        DeferredLightingPassRc_EmissiveAndMetallicTexture,
        DeferredLightingPassRc_WorldNormalTexture,
        DeferredLightingPassRc_VelocityBuffer,
        DeferredLightingPassRc_DepthStencilTexture,
        DeferredLightingPassRc_PointLightBuffer,
        DeferredLightingPassRc_ShadowVisibilityBuffer,
    };

    enum EnvironmentPassRc : uint32_t
    {
        EnvironmentPassRc_CubeMapTexture,
    };

    enum FullScreenDebugRc : uint32_t
    {
        FullScreenDebugRc_AlbedoAndRoughnessTexture,
        FullScreenDebugRc_EmissiveAndMetallicTexture,
        FullScreenDebugRc_WorldNormalTexture,
        FullScreenDebugRc_VelocityBuffer,
        FullScreenDebugRc_ViewDepthBuffer,
        FullScreenDebugRc_DepthBuffer,
        FullScreenDebugRc_ShadowVisibilityBuffer,
        FullScreenDebugRc_TemporalAccumulationBuffer,
        FullScreenDebugRc_ReprojectedHistoryTexture,
        FullScreenDebugRc_DenoisedShadowVisibilityBuffer,

        FullScreenDebugRc_SigmaTiles,
        FullScreenDebugRc_SigmaSmoothTiles,
    };

    enum EquirectangularToCubePassRc : uint32_t
    {
        EquirectangularToCubeRc_EquirectangularTexture,
        EquirectangularToCubeRc_OutCubeTexture,
    };

    enum TestInstancesRc : uint32_t
    {
        TestInstancesRc_TransformBuffer,
        TestInstancesRc_ActiveBitSlotBuffer,
    };

} // namespace joint
