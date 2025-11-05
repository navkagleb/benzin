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
        float m_MinLogLuminance;
        float m_LogLuminanceRange;
        float m_InvLogLuminanceRange;
        float m_TimeFactor; // Tau
    };

    struct PbrCameraConsts
    {
        float m_Aperture;
        float m_ShutterSpeed;
        float m_Iso;
    };

    struct ToneMappingConsts
    {
        LuminanceHistogramConsts m_LuminanceHistogram;
        PbrCameraConsts m_PbrCamera;

        ToneReproductionTransform m_ToneReproductionTransform;

        uint m_IsToneMappingEnabled : 1;
        uint m_IsAutoExposureUsed : 1;
        uint m_IsAccurateGammaCorrectionUsed : 1;
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

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType)
    #define BenzinRenderPassConstsType joint::ToneMappingConsts
#endif
