#pragma once

float CalcEv100(float aperture, float shutterTime, float iso)
{
    // EV number is defined as:
    //   EV_s = log2(N^2 / t)
    //   EV_s = EV_100 + log2(S / 100)
    //
    // This gives
    //   EV_100 + log2(S / 100) = log2(N^2 / t)
    //   EV_100 = log2 (N^2 / t) - log2(S / 100)
    //   EV_100 = log2 (N^2 / t * 100 / S)

    return log2((aperture * aperture) / shutterTime * 100 / iso);
}

float CalcEv100FromAvgLuminance(float avgLuminance)
{
    // We later use the middle gray at 12.7% in order to have
    // a middle gray at 18% with a sqrt (2) room for specular highlights
    // But here we deal with the spot meter measuring the middle gray
    // which is fixed at 12.5 for matching standard camera
    // constructor settings (i.e. calibration constant K = 12.5)
    // Reference : http://en.wikipedia.org/wiki/Film_speed

    const float k = 12.5; // Reflected light calibration constant

    return log2(avgLuminance * 100.0 / k);
}

float Ev100ToExposure(float ev100)
{
    // Compute the maximum luminance possible with H_sbs sensitivity
    // maxLum = (78 / (S * q)) * (N^2 / (t)
    // = 78 / (S * q) * 2^EV_100
    // = 78 / (100 * 0.65) * 2^EV_100
    // = 1.2 * 2^EV_100
    // Reference : http://en.wikipedia.org/wiki/Film_speed

    const float maxLuminance = 1.2 * pow(2.0, ev100);
    return 1.0 / maxLuminance;
}
