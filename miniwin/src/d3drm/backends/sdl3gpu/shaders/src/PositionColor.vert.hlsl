#include "Common.hlsl"

cbuffer ViewportUniforms : register(b0, space1)
{
	float4x4 perspective;
}

FS_Input main(VS_Input input)
{
	FS_Input output;
	output.TexCoord = input.TexCoord;
	output.Color = input.Color;
	output.Normal = input.Normal;
	output.Position = mul(perspective, float4(input.Position, 1.0));
	output.WorldPosition = input.Position;
	output.TexId = input.TexId;
	output.Shininess = input.Shininess;

	return output;
}
