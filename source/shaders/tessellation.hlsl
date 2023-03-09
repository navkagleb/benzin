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
    VS_Output output = (VS_Output) 0;
    output.PositionL = input.PositionL;
    
    return output;
}

struct PatchTessellation
{
    float EdgesTessellation[4] : SV_TessFactor;
    float InsideTessellation[2] : SV_InsideTessFactor;
};

PatchTessellation HS_Constant(InputPatch<VS_Output, 4> patch, uint patchID : SV_PrimitiveID)
{
    const float3 centerL = 0.25f * (patch[0].PositionL + patch[1].PositionL + patch[2].PositionL + patch[3].PositionL);
    const float3 centerW = mul(float4(centerL, 1.0f), g_Object.World).xyz;
    const float distanceToCamera = distance(centerW, g_Pass.CameraPosition);
    
    const float distanceMin = 30.0f;
    const float distanceMax = 100.0f;
    
    const float tessellationFactor = 64.0f * saturate((distanceMax - distanceToCamera) / (distanceMax - distanceMin));
    
    PatchTessellation patchTessellation = (PatchTessellation) 0;
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
[outputcontrolpoints(4)]
[patchconstantfunc("HS_Constant")]
[maxtessfactor(64.0f)]
HS_Output HS_Main(InputPatch<VS_Output, 4> inputPatch, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
    HS_Output output = (HS_Output) 0;
    output.PositionL = inputPatch[i].PositionL;

    return output;
}

struct DS_Output
{
    float4 PositionH : SV_Position;
};

[domain("quad")]
DS_Output DS_Main(PatchTessellation patchTessellation, float2 domainLocation : SV_DomainLocation, const OutputPatch<HS_Output, 4> quad)
{
    // Bilinear interpolation for quad
    // 
    // Q0--x---x---Q1--> U
    // |   |   |   |
    // x---x---x---x
    // |   |   |   |
    // x---x---x---x
    // |   |   |   |
    // Q2--x---x---Q3
    // |
    // v
    // V
    const float3 interpolatedQ0Q1ByX = lerp(quad[0].PositionL, quad[1].PositionL, domainLocation.x);
    const float3 interpolatedQ2Q3ByX = lerp(quad[2].PositionL, quad[3].PositionL, domainLocation.x);
    const float3 positionL = lerp(interpolatedQ0Q1ByX, interpolatedQ2Q3ByX, domainLocation.y);
    
    float4 positionW = mul(float4(positionL, 1.0f), g_Object.World);
    
    // Displacement mapping
    positionW.y = 0.2f * (positionW.z * sin(positionW.x) + positionW.x * cos(positionW.z));
    
    DS_Output output = (DS_Output) 0;
    output.PositionH = mul(positionW, g_Pass.ViewProjection);
    
    return output;
}

float4 PS_Main(DS_Output input) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}