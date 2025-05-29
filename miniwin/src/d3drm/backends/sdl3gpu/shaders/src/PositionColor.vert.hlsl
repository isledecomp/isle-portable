#include "Common.hlsl"

cbuffer ViewportUniforms : register(b0, space1)
{
	float4x4 perspective;
}

cbuffer LightBuffer : register(b1, space1)
{
	SceneLight lights[3];
	int lightCount;
}

FS_Input main(VS_Input input)
{
	FS_Input output;
	output.Color = input.Color;
	output.Normal = input.Normal;
	output.Position = mul(perspective, float4(input.Position, 1.0));
	return output;
}
