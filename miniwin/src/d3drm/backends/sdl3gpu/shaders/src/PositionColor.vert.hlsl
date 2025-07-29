#include "Common.hlsl"

cbuffer ViewportUniforms : REGISTER_SPACE(b0, space1)
{
	float4x4 projection;
	float4x4 worldViewMatrix;
	float4x4 normalMatrix;
}

FS_Input main(VS_Input input)
{
	FS_Input output;
	float3 viewPos = mul(worldViewMatrix, float4(input.Position, 1.0)).xyz;
	output.WorldPosition = viewPos;
	output.Position = mul(projection, float4(viewPos, 1.0));
	output.Normal = normalize(mul(input.Normal, (float3x3) normalMatrix));
	output.TexCoord = input.TexCoord;

	return output;
}
