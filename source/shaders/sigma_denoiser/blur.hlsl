#include "joint/sigma_denoiser_resources.hpp"

#define RenderPassConstantsType joint::SigmaConstants
#include "unified_root_parameters.hlsli"

#include "gbuffer.hlsli"
#include "sigma_denoiser/sigma_common.hlsli"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_WorldNormalTex, joint::SigmaBlurRc_WorldNormalTex);
BenzinDeclareRootResource(Texture2D<float>, g_DepthTex, joint::SigmaBlurRc_DepthTex);
BenzinDeclareRootResource(Texture2D<float>, g_ViewDepthTex, joint::SigmaBlurRc_ViewDepthTex);
BenzinDeclareRootResource(Texture2D<float4>, g_AlbedoAndRoughnessTex, joint::SigmaBlurRc_AlbedoAndRoughnessTex);
BenzinDeclareRootResource(Texture2D<float>, g_PenumbraTex, joint::SigmaBlurRc_PenumbraTex);
BenzinDeclareRootResource(Texture2D<float2>, g_SmoothTilesTex, joint::SigmaBlurRc_SmoothTilesTex);
BenzinDeclareRootResource(Texture2D<float>, g_HistoryTex, joint::SigmaBlurRc_HistoryTex);
BenzinDeclareRootResource(RWTexture2D<float>, g_OutDenoisedPenumbraTex, joint::SigmaBlurRc_OutDenoisedPenumbraTex);
BenzinDeclareRootResource(RWTexture2D<float>, g_OutHistoryTex, joint::SigmaBlurRc_OutHistoryTex);

static const uint g_GroupSizeX = 8; // == g_ThreadCount
static const uint g_GroupSizeY = 16;

static const uint g_BufferSizeX = g_GroupSizeX + SIGMA_BORDER * 2;
static const uint g_BufferSizeY = g_GroupSizeY + SIGMA_BORDER * 2;

struct LdsData
{
    float Penumbra;
    float ViewDepth;
    float Shadow;
};

groupshared LdsData g_Data[g_BufferSizeY][g_BufferSizeX];

struct BlurPreload
{
    void Preload(uint2 localPos, uint2 globalPos)
    {
        const float2 uv = (globalPos + 0.5) * g_FrameConstants.InvRenderResolution;

        LdsData data;
        data.Penumbra = g_PenumbraTex.SampleLevel(g_PointClampSampler, uv, 0.0);
        data.ViewDepth = g_ViewDepthTex.SampleLevel(g_PointClampSampler, uv, 0.0);
        data.Shadow = sigma::IsLit(data.Penumbra); // TODO: Rewrite
    }
};

struct CsInput
{
    uint2 ThreadPos : SV_GroupThreadID;
    uint2 PixelPos : SV_DispatchThreadID;
    uint FlatThreadIndex : SV_GroupIndex;
};

[numthreads(g_GroupSizeX, g_GroupSizeY, 1)]
void CsMain(CsInput input)
{
    const bool isSky = g_SmoothTilesTex[input.PixelPos >> 4].y;

    if (!isSky)
    {
        sigma::LdsDistributor distributor;
        distributor.ThreadPos = input.ThreadPos;
        distributor.PixelPos = input.PixelPos;
        distributor.FlatThreadIndex = input.FlatThreadIndex;
        distributor.BorderSize = SIGMA_BORDER;
        distributor.GroupSize = uint2(g_GroupSizeX, g_GroupSizeY);
        distributor.BufferSize = uint2(g_BufferSizeX, g_BufferSizeY);
        distributor.Dimension = g_FrameConstants.RenderResolution;

        BlurPreload preload;
        distributor.Preload(preload);

        GroupMemoryBarrierWithGroupSync();
    }

    if (isSky || any(input.PixelPos >= g_FrameConstants.RenderResolution))
    {
        return;
    }

    const uint2 ldsPos = input.ThreadPos + SIGMA_BORDER;
    const LdsData centerData = g_Data[ldsPos.y][ldsPos.x];

    float centerSignNoL = float(centerData.Penumbra != 0.0);

    // Early out
    if (centerData.ViewDepth > sigma::g_DenoisingRange)
    {
        return;
    }

    if (g_PassConstants.StabilizationStrength != 0)
    {
        g_OutHistoryTex[input.PixelPos] = g_HistoryTex[input.PixelPos];
    }

    // Tile-based early out ( potentially )
    const float2 pixelUv = DispatchThreadIdToUv(input.PixelPos, g_FrameConstants.InvRenderResolution);

    float tileValue = sigma::TextureCubic(g_SmoothTilesTex, pixelUv);
    tileValue *= all(input.PixelPos < g_FrameConstants.RenderResolution); // due to USE_MAX_DIMS

    if (tileValue == 0.0 || centerData.Penumbra == 0.0)
    {
        g_OutDenoisedPenumbraTex[input.PixelPos] = 0.0;

        // TODO !!!
        // gOut_Shadow_Translucency[ pixelPos ] = PackShadow( s_Shadow_Translucency[ smemPos.y ][ smemPos.x ] ); 
        return;
    }

    // Position
    const float3 viewPos = ReconstructViewPositionFromViewDepth(pixelUv, centerData.ViewDepth, g_FrameConstants.Camera.PackedFrustumPlaneSlopes);

    // Normal
    const float3 worldNormal = g_WorldNormalTex[input.PixelPos].xyz;
    const float3 viewNormal = mul(worldNormal, (float3x3)g_FrameConstants.Camera.WorldToView);

    // Parameters
    const float frustumSize = sigma::GetFrustumSizeAtDepth(
        g_FrameConstants.PixelToWorldScale,
        min(g_FrameConstants.RenderResolution.x, g_FrameConstants.RenderResolution.y),
        centerData.ViewDepth
    );
    const float2 geometryWeightParams = sigma::GetGeometryWeightParams(sigma::g_PlaneDistanceSensitivity, frustumSize, viewPos, viewNormal, 1.0);

#if 1
    // Estimate average distance to occluder
    float2 weight2Sum = 0.0;
    float blurredShadow = 0.0;
    float blurredPenumbra = 0.0;

    [unroll]
    for (int j = 0; j <= SIGMA_BORDER * 2; ++j)
    {
        [unroll]
        for (int i = 0; i <= SIGMA_BORDER * 2; ++i)
        {
            const int2 pos = input.ThreadPos + uint2(i, j);

            const LdsData sampleData = g_Data[pos.y][pos.x];
            const float sampleSignNoL = float(sampleData.Penumbra != 0);

            float weight = 1.0;

            const bool isCenterSample = i == SIGMA_BORDER && j == SIGMA_BORDER;
            if (!isCenterSample)
            {
                const float2 sampleUv = pixelUv + float2(i - SIGMA_BORDER, j - SIGMA_BORDER) * g_FrameConstants.InvRenderResolution;
                const float3 sampleViewPosition = ReconstructViewPositionFromViewDepth(sampleUv, sampleData.ViewDepth, g_FrameConstants.Camera.PackedFrustumPlaneSlopes);
                const float NoX = dot(viewNormal, sampleViewPosition);

                weight = sigma::ComputeWeight(NoX, geometryWeightParams.x, geometryWeightParams.y);
                weight *= sigma::GetGaussianWeight(length(float2(i - SIGMA_BORDER, j - SIGMA_BORDER) / SIGMA_BORDER));
                weight *= (float)(sampleData.ViewDepth < sigma::g_DenoisingRange);
                weight *= (float)(centerSignNoL == sampleSignNoL);
            }

            const float shadow = weight == 0.0 ? 0.0 : sampleData.Shadow;

            float2 weight2 = weight;
            weight2.y *= !sigma::IsLit(sampleData.Penumbra);
            weight2.y *= 1.0 / (1.0 + sampleData.Penumbra * sigma::g_PenumbraWeightScale); // Prefer smaller penumbra

            blurredShadow += sampleData.Shadow * weight2.x;
            blurredPenumbra += sampleData.Penumbra * weight2.y;
            weight2Sum += weight2;
        }
    }

    blurredShadow /= weight2Sum.x; // TODO: lerp to center if blur radius < BORDER
    blurredPenumbra /= max(weight2Sum.y, sigma::g_Eps); // Yes, without patching
#endif

    const float invHitDist = 1.0 / max(blurredPenumbra, sigma::g_Eps);

#if 0
    // Tangent basis with anisotropy
    const float3x3 worldToLocal = sigma::GetOrthonormalBasisFromNormal(viewNormal);
    float3 Tv = mWorldToLocal[ 0 ];
    float3 Bv = mWorldToLocal[ 1 ];

    float3 t = cross( gLightDirectionView.xyz, Nv ); // TODO: add support for other light types to bring proper anisotropic filtering
    if( length( t ) > 0.001 )
    {
        Tv = normalize( t );
        Bv = cross( Tv, Nv );

        float cosa = abs( dot( Nv, gLightDirectionView.xyz ) );
        float skewFactor = lerp( 0.25, 1.0, cosa );

        //Tv *= skewFactor; // TODO: let's not srink filtering in the other direction
        Bv /= skewFactor;
    }
#endif
}
