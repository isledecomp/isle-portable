#include "Common.hlsl"

cbuffer ViewportUniforms : register(b0, space1)
{
	float4x4 projection;
	float4x4 viewMatrix;
}

FS_Input main(VS_Input input)
{
	FS_Input output;
	float3 viewPos = mul(viewMatrix, float4(input.Position, 1.0)).xyz;
	output.WorldPosition = viewPos;
	output.Position = mul(projection, float4(viewPos, 1.0));
	output.Normal = input.Normal;
	output.TexCoord = input.TexCoord;

	return output;
}
