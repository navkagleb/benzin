#include "common.hlsli"

uint GetPassBufferIndex() { return g_RootConstants.Constant0; }
uint GetPassBufferOffset() { return g_RootConstants.Constant1; }

uint GetEntityBufferIndex() { return g_RootConstants.Constant2; }
uint GetEntityBufferOffset() { return g_RootConstants.Constant3; }

#if defined(ALPHA_TEST)
uint GetMaterialBufferIndex() { return g_RootConstants.Constant4; }
#endif

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
	const StructuredBuffer<PassData> passBuffer = ResourceDescriptorHeap[GetPassBufferIndex()];
	const StructuredBuffer<EntityData> entityBuffer = ResourceDescriptorHeap[GetEntityBufferIndex()];

	const PassData passData = passBuffer[GetPassBufferOffset() + 0];
	const EntityData entityData = entityBuffer[GetEntityBufferOffset() + instanceID];

	VS_Output output = (VS_Output)0;

	const float4 positionW = mul(float4(input.PositionL, 1.0f), entityData.World);
	output.PositionH = mul(positionW, passData.ViewProjection);

#if defined(ALPHA_TEST)
	const StructuredBuffer<MaterialData> materialBuffer = ResourceDescriptorHeap[GetMaterialBufferIndex()];

	const MaterialData materialData = materialBuffer[entityData.MaterialIndex];

	output.TexCoord = mul(float4(input.TexCoord, 0.0f, 1.0f), materialData.Transform).xy;
	output.MaterialIndex = entityData.MaterialIndex;
#endif

	return output;
}

void PS_Main(VS_Output input)
{
#if defined(ALPHA_TEST)
	const StructuredBuffer<MaterialData> materialBuffer = ResourceDescriptorHeap[GetMaterialBufferIndex()];

	const MaterialData materialData = materialBuffer[input.MaterialIndex];
	const Texture2D diffuseMap = ResourceDescriptorHeap[materialData.DiffuseMapIndex];

	const float4 diffuseAlbedo = material.DiffuseAlbedo * diffuseMap.Sample(g_LinearWrapSampler, input.TexCoord);

	clip(diffuseAlbedo.a - 0.1f);
#endif // #if defined(ALPHA_TEST)
}
