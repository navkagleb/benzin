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
    };

    enum EquirectangularToCubePassRc : uint32_t
    {
        EquirectangularToCubeRc_EquirectangularTexture,
        EquirectangularToCubeRc_OutCubeTexture,
    };

}
