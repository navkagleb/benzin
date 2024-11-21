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

    enum RayTracingShadowsRc : uint32_t
    {
        RayTracingShadowsRc_WorldNormalTex,
        RayTracingShadowsRc_DepthTex,

        RayTracingShadowsRc_OutNoisyPenumbraTex,
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
        DeferredLightingPassRc_AlbedoAndRoughnessTex,
        DeferredLightingPassRc_EmissiveAndMetallicTex,
        DeferredLightingPassRc_WorldNormalTex,
        DeferredLightingPassRc_VelocityTex,
        DeferredLightingPassRc_DepthStencilTex,
        DeferredLightingPassRc_PointLightBuf,
        DeferredLightingPassRc_SigmaShadowTex,
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
        FullScreenDebugRc_NoisyPenumbraTexture,

        FullScreenDebugRc_SigmaTiles,
        FullScreenDebugRc_SigmaSmoothTiles,
        FullScreenDebugRc_SigmaPenumbra1,
        FullScreenDebugRc_SigmaPenumbra2,
        FullScreenDebugRc_SigmaShadowTemp1,
        FullScreenDebugRc_SigmaShadowTemp2,
        FullScreenDebugRc_SigmaShadow,
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
