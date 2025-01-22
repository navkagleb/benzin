#define g_ThreadCountX 8
#define g_ThreadCountY 16

#define SIGMA_USE_BORDER_2

#include "joint/sigma_denoiser_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "sigma_denoiser/group_shared_preloader.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_WorldNormal, joint::Rc_SigmaBlur::WorldNormal);
BenzinDeclareRootResource(Texture2D<float>, g_ViewDepth, joint::Rc_SigmaBlur::ViewDepth);
BenzinDeclareRootResource(Texture2D<float>, g_Penumbra, joint::Rc_SigmaBlur::Penumbra);
BenzinDeclareRootResource(Texture2D<float2>, g_SmoothTiles, joint::Rc_SigmaBlur::SmoothTiles);
#if defined(POST_BLUR_PASS)
    BenzinDeclareRootResource(Texture2D<float>, g_Shadow, joint::Rc_SigmaBlur::Shadow);
#endif

BenzinDeclareRootResource(RWTexture2D<float>, g_OutPenumbra, joint::Rc_SigmaBlur::OutPenumbra);
BenzinDeclareRootResource(RWTexture2D<float>, g_OutShadow, joint::Rc_SigmaBlur::OutShadow);

struct PixelData
{
    float Penumbra;
    float ViewDepth;
    float Shadow;
};

groupshared PixelData g_PixelsData[g_SharedBufferSizeY][g_SharedBufferSizeX];

void Preload(uint2 sharedPos, uint2 pixelPos)
{
    PixelData pixel;
    pixel.Penumbra = g_Penumbra[pixelPos];
    pixel.ViewDepth = g_ViewDepth[pixelPos];

#if !defined(POST_BLUR_PASS)
    pixel.Shadow = sigma::IsLit(pixel.Penumbra); // This is ok. Full shadow - 0, No shadow = 1
#else
    pixel.Shadow = sigma::UnpackShadow(g_Shadow[pixelPos]);
#endif

    g_PixelsData[sharedPos.y][sharedPos.x] = pixel;
}

struct CsInput
{
    uint2 ThreadPos : SV_GroupThreadID;
    uint2 PixelPos : SV_DispatchThreadID;
    uint FlatThreadIndex : SV_GroupIndex;
};

struct BlurParams
{
    float2 UvToViewScale;
    float2 UvToViewBias;

    PixelData CenterPixel;

    float2 BaseUv;
    float3 BaseViewPosition;
    float3 BaseViewNormal;

    float WorldPixelSize;
    float2 GeometryWeightParams;
};

struct SampleParams
{
    float3 ViewPosition;
    float NormDistanceFromCenter;
};

struct SparseBlurKernel
{
    float3 Tangent;
    float3 Bitangent;
    float4 Rotator; // 2x2 matrix
};

BlurParams GetBlurParams(float2 baseUv, PixelData centerPixel)
{
    const joint::CameraConsts camera = g_FrameConstants.Camera;
    const float pixelToWorldScale = g_FrameConstants.Camera.PixelToWorldScale;
    const float3 worldNormal = g_WorldNormal.SampleLevel(g_PointClampSampler, baseUv, 0.0).xyz;
    const float worldFrustumSize = sigma::PixelsToWorldSize(g_FrameConstants.MinRenderDimension, pixelToWorldScale, centerPixel.ViewDepth);

    BlurParams params;
    params.UvToViewScale = camera.UvToViewScale;
    params.UvToViewBias = camera.UvToViewBias;
    params.CenterPixel = centerPixel;
    params.BaseUv = baseUv;
    params.BaseViewPosition = ReconstructViewPosition(baseUv, centerPixel.ViewDepth, params.UvToViewScale, params.UvToViewBias);
    params.BaseViewNormal = mul(worldNormal, (float3x3)camera.WorldToView);
    params.WorldPixelSize = sigma::GetWorldPixelSize(pixelToWorldScale, centerPixel.ViewDepth);
    params.GeometryWeightParams = sigma::GetGeometryWeightParams(g_PassConsts0.PlaneDistanceSensitivity, params.BaseViewPosition, params.BaseViewNormal, worldFrustumSize);

    return params;
}

float CalcShadowWeight(BlurParams params, PixelData samplePixel, SampleParams sampleParams)
{
    float shadowWeight = 1.0;
    shadowWeight *= sigma::GetGaussianWeight(sampleParams.NormDistanceFromCenter);
    shadowWeight *= sigma::CalcGeometryWeight(params.BaseViewNormal, sampleParams.ViewPosition, params.GeometryWeightParams);
    shadowWeight *= (float)sigma::IsBothLitOrUmbra(params.CenterPixel.Penumbra, samplePixel.Penumbra);

    return shadowWeight;
}

float CalcPenumbraWeight(BlurParams params, float shadowWeight, PixelData samplePixel)
{
    float penumbraWeight = shadowWeight;
    penumbraWeight *= params.WorldPixelSize / (params.WorldPixelSize + samplePixel.Penumbra); // Prefer smaller penumbra
    penumbraWeight *= !sigma::IsLit(samplePixel.Penumbra);

    return penumbraWeight;
}

SparseBlurKernel CalcSparseBlurKernel(BlurParams params, float blurredPenumbra, float tileValue)
{
    // Tangent basis with anisotropy
    const float3x3 worldToLocal = sigma::GetOrthonormalBasisFromNormal(params.BaseViewNormal); // TODO: ViewNormal???

    SparseBlurKernel kernel;
    kernel.Tangent = worldToLocal[0];
    kernel.Bitangent = worldToLocal[1];
#if !defined(POST_BLUR_PASS)
    kernel.Rotator = g_PassConsts0.BlurRotator;
#else
    kernel.Rotator = g_PassConsts0.PostBlurRotator;
#endif

    float3 worldToLightDirection;
    switch (g_PassConsts1.LightType)
    {
        case joint::LightType::Sun:
        {
            worldToLightDirection = g_PassConsts1.WorldLightPosition;
            break;
        }
        case joint::LightType::Spherical:
        {
            const float3 worldPosition = mul(float4(params.BaseViewPosition, 1.0), g_FrameConstants.Camera.ViewToWorld).xyz;
            worldToLightDirection = normalize(worldPosition - g_PassConsts1.WorldLightPosition);

            break;
        }
    }

    const float3 viewToLightDirection = mul(worldToLightDirection, (float3x3)g_FrameConstants.Camera.WorldToView); // TODO: Move to cpp side
    const float3 tangentDirection = cross(viewToLightDirection, params.BaseViewNormal); // NRD TODO: add support for other light types to bring proper anisotropic filtering
    if (length(tangentDirection) > 0.001)
    {
        kernel.Tangent = normalize(tangentDirection);
        kernel.Bitangent = cross(kernel.Tangent, params.BaseViewNormal);

        const float cosNormalSun = abs(dot(params.BaseViewNormal, viewToLightDirection));
        const float skewFactor = lerp(0.25, 1.0, cosNormalSun);

        // kernel.Tangent *= skewFactor; // TODO: let's not srink filtering in the other direction
        kernel.Bitangent /= skewFactor;
    }

    const float pixelRadius = sigma::GetKernelPixelRadius(blurredPenumbra, params.WorldPixelSize, tileValue);
    const float worldPixelRadius = pixelRadius * params.WorldPixelSize; //TODO: Why we multipy pixelRadius by worldPixelSize

    kernel.Tangent *= worldPixelRadius;
    kernel.Bitangent *= worldPixelRadius;

    return kernel;
}

float2 CalcSparseBlurKernelUv(SparseBlurKernel kernel, float2 offset, float3 viewPosition)
{
    // We can't rotate T and B instead, because T is skewed
    offset.xy = sigma::RotateVectorByRotator(offset, kernel.Rotator);

    viewPosition += offset.x * kernel.Tangent + offset.y * kernel.Bitangent;

    const float4x4 viewToClip = g_FrameConstants.Camera.ViewToClip;

    const float4 clipPos = mul(float4(viewPosition, 1.0), viewToClip); // TODO: Why this don't work?
    // const float4 clipPos = mul(g_FrameConstants.Camera.ViewToClip, float4(viewPosition, 1.0));
    const float2 uv = ClipToUv(clipPos);

    return uv;
}

void RunIsotropicBlur(sigma::GroupSharedCsInput input, BlurParams params, out float2 outShadow, out float2 outPenumbra)
{
    outShadow = 0.0;
    outPenumbra = 0.0;

    [unroll]
    for (int j = -SIGMA_BORDER; j <= SIGMA_BORDER; ++j)
    {
        [unroll]
        for (int i = -SIGMA_BORDER; i <= SIGMA_BORDER; ++i)
        {
            const uint2 sharedPosition = input.ThreadPos + int2(i, j) + SIGMA_BORDER;
            const PixelData pixel = g_PixelsData[sharedPosition.y][sharedPosition.x];

            float shadowWeight = 1.0;

            const bool isCenterSample = i == 0 && j == 0;
            if (!isCenterSample)
            {
                const float2 pixelOffset = float2(i, j);
                const float2 uv = params.BaseUv + pixelOffset * g_FrameConstants.InvRenderResolution;

                SampleParams sampleParams;
                sampleParams.ViewPosition = ReconstructViewPosition(uv, pixel.ViewDepth, params.UvToViewScale, params.UvToViewBias);
                sampleParams.NormDistanceFromCenter = length(pixelOffset / SIGMA_BORDER);

                shadowWeight = CalcShadowWeight(params, pixel, sampleParams);
            }

            const float penumbraWeight = CalcPenumbraWeight(params, shadowWeight, pixel);

            outShadow += float2(pixel.Shadow, 1.0) * shadowWeight;
            outPenumbra += float2(pixel.Penumbra, 1.0) * penumbraWeight;
        }
    }

    outShadow.x /= outShadow.y;
    outShadow.y = 1.0;

    outPenumbra.x /= max(outPenumbra.y, sigma::g_Eps); // Yes, without patching // TODO: What it means?
    outPenumbra.y = outPenumbra.y != 0.0;
}

void RunAnisotropicBlur(BlurParams params, float tileValue, inout float2 outShadow, inout float2 outPenumbra)
{
    // World space sampling

    const SparseBlurKernel sparseKernel = CalcSparseBlurKernel(params, outPenumbra.x, tileValue);

    const float invEstimatedPenumbra = 1.0 / max(outPenumbra.x, sigma::g_Eps);

    for (uint sampleIndex = 0; sampleIndex < SIGMA_BLUR_POISSON_SAMPLE_COUNT; ++sampleIndex)
    {
        const float3 offset = SIGMA_BLUR_POISSON_SAMPLES[sampleIndex]; // TODO: Name this variable with prefix

        float2 uv = CalcSparseBlurKernelUv(sparseKernel, offset.xy, params.BaseViewPosition);
        uv = (floor(uv * g_FrameConstants.RenderResolution) + 0.5) * g_FrameConstants.InvRenderResolution; // Snap to the pixel center

        const uint2 pixelPosition = uv * g_FrameConstants.RenderResolution;

        PixelData samplePixel;
        samplePixel.ViewDepth = g_ViewDepth.SampleLevel(g_PointClampSampler, uv, 0.0);
        samplePixel.Penumbra = g_Penumbra.SampleLevel(g_PointClampSampler, uv, 0.0);
#if !defined(POST_BLUR_PASS)
        samplePixel.Shadow = sigma::IsLit(samplePixel.Penumbra);
#else
        samplePixel.Shadow = g_Shadow.SampleLevel(g_PointClampSampler, uv, 0.0);
        samplePixel.Shadow = sigma::UnpackShadow(samplePixel.Shadow);
#endif

        SampleParams sampleParams;
        sampleParams.ViewPosition = ReconstructViewPosition(uv, samplePixel.ViewDepth, params.UvToViewScale, params.UvToViewBias);
        sampleParams.NormDistanceFromCenter = offset.z;

        float shadowWeight = sigma::IsUvIn01Range(uv);
        shadowWeight *= CalcShadowWeight(params, samplePixel, sampleParams);

        // Avoid umbra leaking inside wide penumbra
        // NRD TODO: it works surprisingly well, keep an eye on it!
        shadowWeight *= saturate(samplePixel.Penumbra * invEstimatedPenumbra); 

        const float penumbraWeight = CalcPenumbraWeight(params, shadowWeight, samplePixel);

        outShadow += float2(samplePixel.Shadow, 1.0) * shadowWeight;
        outPenumbra += float2(samplePixel.Penumbra, 1.0) * penumbraWeight;
    }

    outShadow.x /= outShadow.y;
    outPenumbra.x = outPenumbra.y == 0.0 ? params.CenterPixel.Penumbra : outPenumbra.x / outPenumbra.y;
}

[numthreads(g_ThreadCountX, g_ThreadCountY, 1)]
void CsMain(sigma::GroupSharedCsInput input)
{
    bool isSky = SIGMA_USE_TILE_CHECK;
    isSky = isSky && g_SmoothTiles[input.PixelPos >> 4].y;

    if (!isSky)
    {
        SigmaPreloadToGroupSharedMem(input, g_FrameConstants.RenderResolution, Preload);
    }

    GroupMemoryBarrierWithGroupSync();

    if (isSky || any(input.PixelPos >= g_FrameConstants.RenderResolution))
    {
        return;
    }

    const uint2 sharedPos = input.ThreadPos + SIGMA_BORDER;
    const PixelData centerPixel = g_PixelsData[sharedPos.y][sharedPos.x];

    if (centerPixel.ViewDepth > SIGMA_DENOISING_RANGE)
    {
        return;
    }

    // Tile-based early out (potentially)
    const float2 pixelUv = (input.PixelPos + 0.5) * g_FrameConstants.InvRenderResolution;
    const float tileValue = sigma::TextureCubicX(g_SmoothTiles, pixelUv);

    const bool isHardShadow = (SIGMA_USE_TILE_CHECK && tileValue == 0.0) || centerPixel.Penumbra == 0.0;
    if (isHardShadow)
    {
        g_OutPenumbra[input.PixelPos] = centerPixel.Penumbra;
        g_OutShadow[input.PixelPos] = sigma::PackShadow(centerPixel.Shadow);

        return;
    }

    const BlurParams params = GetBlurParams(pixelUv, centerPixel);
    
    float2 blurredShadow = 0.0;
    float2 blurredPenumbra = 0.0;

    RunIsotropicBlur(input, params, blurredShadow, blurredPenumbra);

    // Avoid blurry result if penumbra size < BORDER
    const float penumbraInPixels = blurredPenumbra.x / params.WorldPixelSize;
    const float factor = smoothstep(0.0, SIGMA_BORDER, penumbraInPixels);
    blurredShadow.x = lerp(params.CenterPixel.Shadow, blurredShadow.x, factor); // TODO: not the best solution

    // Avoid unnecessary weight increase for the unfiltered center sample if the blur radius is small
    const float f = lerp(4.0, 1.0, factor); // TODO: adds blurriness
    blurredShadow *= f;
    blurredPenumbra *= f;

#if SIGMA_BLUR_USE_ANISOTROPIC_BLUR
    RunAnisotropicBlur(params, tileValue, blurredShadow, blurredPenumbra); // TODO: Normalize blurredShadow and blurredPenumbra when SIGMA_BLUR_USE_ANISOTROPIC_BLUR is 0
#endif

#if defined(POST_BLUR_PASS)
    if (g_PassConsts0.StabilizationStrength != 0)
#endif
    {
        g_OutPenumbra[input.PixelPos] = blurredPenumbra.x;
    }

    g_OutShadow[input.PixelPos] = sigma::PackShadow(blurredShadow.x);
}
