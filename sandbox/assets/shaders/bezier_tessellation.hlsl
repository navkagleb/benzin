struct PassConstantBuffer
{
    float4x4 View;
    float4x4 Projection;
    float4x4 ViewProjection;
    float3 CameraPosition;
};

struct ObjectConstantBuffer
{
    float4x4 World;
};

ConstantBuffer<PassConstantBuffer> g_Pass : register(b0);
ConstantBuffer<ObjectConstantBuffer> g_Object : register(b1);

struct VS_Input
{
    float3 PositionL : Position;
};

struct VS_Output
{
    float3 PositionL : Position;
};

VS_Output VS_Main(VS_Input input)
{
    VS_Output output = (VS_Output)0;
    output.PositionL = input.PositionL;

    return output;
}

struct PatchTessellation
{
    float EdgesTessellation[4] : SV_TessFactor;
    float InsideTessellation[2] : SV_InsideTessFactor;
};

PatchTessellation HS_Constant(InputPatch<VS_Output, 16> patch, uint patchID : SV_PrimitiveID)
{
    const float3 centerL = 0.25f * (patch[0].PositionL + patch[1].PositionL + patch[2].PositionL + patch[3].PositionL);
    const float3 centerW = mul(float4(centerL, 1.0f), g_Object.World).xyz;
    const float distanceToCamera = distance(centerW, g_Pass.CameraPosition);

    const float distanceMin = 30.0f;
    const float distanceMax = 100.0f;

    const float tessellationFactor = 64.0f * saturate((distanceMax - distanceToCamera) / (distanceMax - distanceMin));

    PatchTessellation patchTessellation = (PatchTessellation)0;
    patchTessellation.EdgesTessellation[0] = tessellationFactor;
    patchTessellation.EdgesTessellation[1] = tessellationFactor;
    patchTessellation.EdgesTessellation[2] = tessellationFactor;
    patchTessellation.EdgesTessellation[3] = tessellationFactor;
    patchTessellation.InsideTessellation[0] = tessellationFactor;
    patchTessellation.InsideTessellation[1] = tessellationFactor;

    return patchTessellation;
}

struct HS_Output
{
    float3 PositionL : Position;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(16)]
[patchconstantfunc("HS_Constant")]
[maxtessfactor(64.0f)]
HS_Output HS_Main(InputPatch<VS_Output, 16> inputPatch, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
    HS_Output output = (HS_Output)0;
    output.PositionL = inputPatch[i].PositionL;

    return output;
}

namespace utils
{

    float4 BernsteinBasis(float t)
    {
        const float inverseT = 1.0f - t;

        const float b0 = inverseT * inverseT * inverseT;
        const float b1 = 3.0f * t * inverseT * inverseT;
        const float b2 = 3.0f * t * t * inverseT;
        const float b3 = t * t * t;

        return float4(b0, b1, b2, b3);
    }

    float4 BernstainBasisDerivative(float t)
    {
        const float inverseT = 1.0f - t;

        const float b0Derivative = -3 * inverseT * inverseT;
        const float b1Derivative = 3 * inverseT * inverseT - 6 * t * inverseT;
        const float b2Derivative = 6 * t * inverseT - 3 * t * t;
        const float b3Derivative = 3 * t * t;

        return float4(b0Derivative, b1Derivative, b2Derivative, b3Derivative);
    }

    float3 CubicBezierSum(const OutputPatch<HS_Output, 16> bezierPatch, float4 basisU, float4 basisV)
    {
        float3 sum = float3(0.0f, 0.0f, 0.0f);
        sum += basisV.x * (basisU.x * bezierPatch[0].PositionL + basisU.y * bezierPatch[1].PositionL + basisU.z * bezierPatch[2].PositionL + basisU.w * bezierPatch[3].PositionL);
        sum += basisV.y * (basisU.x * bezierPatch[4].PositionL + basisU.y * bezierPatch[5].PositionL + basisU.z * bezierPatch[6].PositionL + basisU.w * bezierPatch[7].PositionL);
        sum += basisV.z * (basisU.x * bezierPatch[8].PositionL + basisU.y * bezierPatch[9].PositionL + basisU.z * bezierPatch[10].PositionL + basisU.w * bezierPatch[11].PositionL);
        sum += basisV.w * (basisU.x * bezierPatch[12].PositionL + basisU.y * bezierPatch[13].PositionL + basisU.z * bezierPatch[14].PositionL + basisU.w * bezierPatch[15].PositionL);

        return sum;
    }

} // namespace utils

struct DS_Output
{
    float4 PositionH : SV_Position;
};

[domain("quad")]
DS_Output DS_Main(PatchTessellation patchTessellation, float2 domainLocation : SV_DomainLocation, const OutputPatch<HS_Output, 16> bezierPatch)
{
    const float4 basisU = utils::BernsteinBasis(domainLocation.x);
    const float4 basisV = utils::BernsteinBasis(domainLocation.y);

    const float3 positionL = utils::CubicBezierSum(bezierPatch, basisU, basisV);
    const float4 positionW = mul(float4(positionL, 1.0f), g_Object.World);

    DS_Output output = (DS_Output)0;
    output.PositionH = mul(positionW, g_Pass.ViewProjection);

    return output;
}

float4 PS_Main(DS_Output input) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}