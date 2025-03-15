#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class ToneReproductionTransform : uint
    {
        Linear,
        Reinhard,
        AcesFilm,
        Unreal,
    };

    struct LuminanceHistogramConsts
    {
        float MinLogLuminance;
        float LogLuminanceRange;
        float InvLogLuminanceRange;
        float TimeFactor; // Tau
    };

    struct PbrCameraConsts
    {
        float Aperture;
        float ShutterSpeed;
        float Iso;
    };

    struct ToneMappingConsts
    {
        LuminanceHistogramConsts LuminanceHistogram;
        PbrCameraConsts PbrCamera;

        ToneReproductionTransform ToneReproductionTransform;

        uint IsToneMappingEnabled : 1;
        uint IsAutoExposureUsed : 1;
        uint IsAccurateGammaCorrectionUsed : 1;
    };

    enum class CalcLuminanceHistogramResources : uint
    {
        HdrColor,
        OutLuminanceHistogram,
        OutDebugLuminanceHistogram,
    };

    enum class CalcAvgLuminanceResources : uint
    {
        OutLuminanceHistogram,
        OutAvgLuminance,
    };

    enum class ApplyToneMapOperatorResources : uint
    {
        AvgLuminance,
        HdrColor,
        OutFinal,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::ToneMappingConsts
#endif
