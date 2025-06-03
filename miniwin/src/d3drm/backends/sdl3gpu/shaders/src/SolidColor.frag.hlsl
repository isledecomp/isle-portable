#include "Common.hlsl"

cbuffer LightBuffer : register(b0, space3)
{
	SceneLight lights[3];
	int lightCount;
}

FS_Output main(FS_Input input)
{
	FS_Output output;

	float3 diffuse = float3(0, 0, 0);
	float3 specular = float3(0, 0, 0);

	for (int i = 0; i < lightCount; ++i) {
		float3 lightColor = lights[i].color.rgb;

		if (lights[i].position.w == 0.0 && lights[i].direction.w == 0.0) {
			diffuse += lightColor;
			continue;
		}

		float3 lightVec;
		if (lights[i].direction.w == 1.0) {
			lightVec = normalize(-lights[i].direction.xyz);
		}
		else {
			float3 lightPos = lights[i].position.xyz;
			lightVec = lightPos - input.WorldPosition;

			float len = length(lightVec);
			if (len == 0.0f) {
				continue;
			}

			lightVec /= len;
		}

		float dotNL = dot(input.Normal, lightVec);
		if (dotNL > 0.0f) {
			diffuse += dotNL * lightColor;

			if (input.Shininess != 0.0f) {
				// Using dotNL ignores view angle, but this matches DirectX 5 behavior.
				float spec1 = pow(dotNL, input.Shininess);
				specular += spec1 * lightColor;
			}
		}
	}

	float3 baseColor = input.Color.rgb;
	float3 finalColor = saturate(diffuse * baseColor + specular);

	output.Color = float4(finalColor, input.Color.a);
	output.Depth = input.Position.w;
	return output;
}
