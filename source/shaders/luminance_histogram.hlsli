#pragma once

float LinearToLogLuminance(float linearLuminance)
{
    const float logLuminance = (log2(linearLuminance) - g_PassConsts0.LuminanceHistogram.MinLogLuminance) * g_PassConsts0.LuminanceHistogram.InvLogLuminanceRange;
    return saturate(logLuminance);
}

float LogToLinearLuminance(float logLuminance)
{
    float linearLuminance = (logLuminance * g_PassConsts0.LuminanceHistogram.LogLuminanceRange) + g_PassConsts0.LuminanceHistogram.MinLogLuminance;
    linearLuminance = exp2(linearLuminance);

    return linearLuminance;
}

uint LogLuminanceToBinIndex(float logLuminance)
{
    return logLuminance * 254.0 + 1.0;
}

float BinIndexToLogLuminance(float binIndex)
{
    return (binIndex - 1.0) / 254.0;
}

uint LuminanceToBinIndex(float luminance)
{
    if (luminance < 0.005)
    {
        return 0;
    }

    const float logLuminance = LinearToLogLuminance(luminance);
    return LogLuminanceToBinIndex(logLuminance);
}

float BinIndexToLuminance(float binIndex)
{
    const float logLuminance = BinIndexToLogLuminance(binIndex);
    return LogToLinearLuminance(logLuminance);
}
