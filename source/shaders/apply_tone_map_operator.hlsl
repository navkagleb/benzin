// Ref: RGB/XYZ Matrices - http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
// Ref: XYZ to xyY - http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_xyY.html
// Ref: xyY to XYZ - http://www.brucelindbloom.com/index.html?Eqn_xyY_to_XYZ.html
// Ref: Krzysztof Narkowicz - ACES Filmic Tone Mapping Curve - https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
// Ref: Krzysztof Narkowicz - Automatic Exposure - https://knarkowicz.wordpress.com/2016/01/09/automatic-exposure/
// Ref: Tonemap operators - https://www.shadertoy.com/view/llXyWr

// TODO:
// - Ref: PBR Camera (p 82) - https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/course-notes-moving-frostbite-to-pbr-v2.pdf
// - Ref: Uncharted 2: HDR Lighting - John Hable - https://www.gdcvault.com/play/1012351/Uncharted-2-HDR
// - Ref: Implementing a Physically Based Camera: Understanding Exposure - https://placeholderart.wordpress.com/2014/11/16/implementing-a-physically-based-camera-understanding-exposure/

#include "joint/tone_mapping_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "color_convertions.hlsli"
#include "exposition.hlsli"

BenzinDeclareRootResource(Texture2D<float>, g_AvgLuminance, joint::ApplyToneMapOperatorResources::AvgLuminance);
BenzinDeclareRootResource(Texture2D<float4>, g_HdrColor, joint::ApplyToneMapOperatorResources::HdrColor);
BenzinDeclareRootResource(RWTexture2D<float4>, g_OutFinal, joint::ApplyToneMapOperatorResources::OutFinal);

float3 ApplyExposureCorrection(float3 rgb)
{
    const float manualEv100 = CalcEv100(g_PassConsts0.PbrCamera.Aperture, g_PassConsts0.PbrCamera.ShutterSpeed, g_PassConsts0.PbrCamera.Iso);
    const float autoEv100 = CalcEv100FromAvgLuminance(g_AvgLuminance[uint2(0, 0)]);

    const float exposure = Ev100ToExposure(g_PassConsts0.IsAutoExposureUsed ? autoEv100 : manualEv100);

    return rgb * exposure;
}

float3 ApplyToneReproductionTransform(float3 rgb)
{
    switch (g_PassConsts0.ToneReproductionTransform)
    {
        case joint::ToneReproductionTransform::Linear:
        {
            return saturate(rgb);
        };
        case joint::ToneReproductionTransform::Reinhard:
        {
            return rgb / (rgb + 1.0);
        }
        case joint::ToneReproductionTransform::AcesFilm:
        {
            const float a = 2.51;
            const float b = 0.03;
            const float c = 2.43;
            const float d = 0.59;
            const float e = 0.14;

            return saturate((rgb * (a * rgb + b)) / (rgb * (c * rgb + d) + e));
        }
        case joint::ToneReproductionTransform::Unreal:
        {
            // Unreal 3, Documentation: "Color Grading"
            // Adapted to be close to 'TonemapAcesFilm', with similar range
            // Gamma 2.2 correction is baked in, don't use with sRGB conversion!

            return rgb / (rgb + 0.155) * 1.019;
        }
    }

    return 0.0;
}

float3 ApplyGammaCorrection(float3 rgb)
{
    if (g_PassConsts0.ToneReproductionTransform != joint::ToneReproductionTransform::Unreal)
    {
        rgb = g_PassConsts0.IsAccurateGammaCorrectionUsed ? LinearToSrgbAccurate(rgb) : LinearToSrgb(rgb);
    }

    return rgb;
}

[numthreads(16, 16, 1)]
void CsMain(uint2 pixelPos : SV_DispatchThreadID)
{
    float3 rgb = g_HdrColor[pixelPos].xyz;

    if (g_PassConsts0.IsToneMappingEnabled)
    {
        rgb = ApplyExposureCorrection(rgb);
        rgb = ApplyToneReproductionTransform(rgb);
        rgb = ApplyGammaCorrection(rgb);
    }

    g_OutFinal[pixelPos] = float4(rgb, 1.0);
}
