#include "common.hlsl"

struct PassConstants
{
	float4x4 ViewProjectionMatrix;
};

struct VS_Input
{
	float3 PositionL : Position;

#if defined(ALPHA_TEST)
	float2 TexCoord : TexCoord;
#endif
};

struct VS_Output
{
	float4 PositionH : SV_Position;

#if defined(ALPHA_TEST)
	float2 TexCoord : TexCoord;

	nointerpolation uint MaterialIndex : MaterialIndex;
#endif
};

VS_Output VS_Main(VS_Input input, uint instanceID : SV_InstanceID)
{
	const EntityData entity = g_Entities[instanceID];

	VS_Output output = (VS_Output)0;

	const float4 positionW = mul(float4(input.PositionL, 1.0f), entity.World);
	output.PositionH = mul(positionW, g_Pass.ViewProjection);

#if defined(ALPHA_TEST)
	const MaterialData material = g_Materials[entity.MaterialIndex];

	output.TexCoord = mul(float4(input.TexCoord, 0.0f, 1.0f), material.Transform).xy;
	output.MaterialIndex = entity.MaterialIndex;
#endif

	return output;
}

void PS_Main(VS_Output input)
{
#if defined(ALPHA_TEST)
	const MaterialData material = g_Materials[input.MaterialIndex];
	const Texture2D diffuseMap = g_Textures[NonUniformResourceIndex(material.DiffuseMapIndex)];

	const float4 diffuseAlbedo = material.DiffuseAlbedo * diffuseMap.Sample(g_LinearWrapSampler, input.TexCoord);

	clip(diffuseAlbedo.a - 0.1f);
#endif // #if defined(ALPHA_TEST)
}
