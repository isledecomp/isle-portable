#include "Common.hlsl"

cbuffer LightBuffer : register(b0, space3)
{
	SceneLight lights[3];
	int lightCount;
}

FS_Output main(FS_Input input)
{
	FS_Output output;
	float3 normal = normalize(input.Normal);
	float3 fragPos = input.Position.xyz / input.Position.w;

	float3 viewPos = float3(0, 0, 0);
	float3 viewDir = normalize(viewPos - fragPos);

	float3 result = float3(0, 0, 0);

	const float shininess = 20.0; // All materials use this in Isle

	for (int i = 0; i < lightCount; ++i) {
		float3 lightColor = lights[i].color.rgb;

		bool hasPos = lights[i].position.w == 1.0;
		bool hasDir = lights[i].direction.w == 1.0;

		if (!hasPos && !hasDir) {
			// D3DRMLIGHT_AMBIENT
			result += input.Color.rgb * lightColor;
			continue;
		}

		if (hasPos) {
			// D3DRMLIGHT_POINT
			float3 lightPos = lights[i].position.xyz;
			float3 lightDir = normalize(lightPos - fragPos);
			float diff = max(dot(normal, lightDir), 0.0);
			float distance = length(lightPos - fragPos);
			float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
			float3 halfwayDir = normalize(lightDir + viewDir);
			float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
			result += (input.Color.rgb * diff + spec) * lightColor * attenuation;
			continue;
		}

		if (hasDir) {
			// D3DRMLIGHT_DIRECTIONAL
			float3 lightDir = normalize(-lights[i].direction.xyz);
			float diff = max(dot(normal, lightDir), 0.0);
			float3 halfwayDir = normalize(lightDir + viewDir);
			float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
			result += (input.Color.rgb * diff + spec) * lightColor;
			continue;
		}
	}

	output.Color = float4(result, input.Color.a);
	output.Depth = input.Position.w;
	return output;
}
