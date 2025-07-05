#include "Common.hlsl"

struct FS_Output {
	float4 Color : SV_Target0;
	float Depth : SV_Depth;
};

cbuffer FragmentShadingData : REGISTER_SPACE(b0, space3)
{
	SceneLight lights[3];
	int lightCount;
	float Shininess;
	uint ColorRaw;
	int UseTexture;
}

float4 unpackColor(uint packed)
{
	float4 color;
	color.r = ((packed >> 0) & 0xFF) / 255.0f;
	color.g = ((packed >> 8) & 0xFF) / 255.0f;
	color.b = ((packed >> 16) & 0xFF) / 255.0f;
	color.a = ((packed >> 24) & 0xFF) / 255.0f;
	return color;
}

Texture2D<float4> Texture : REGISTER_SPACE(t0, space2);
SamplerState Sampler : REGISTER_SPACE(s0, space2);

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
			lightVec = -lights[i].direction.xyz;
		}
		else {
			lightVec = lights[i].position.xyz - input.WorldPosition;
		}
		lightVec = normalize(lightVec);

		float dotNL = dot(input.Normal, lightVec);
		if (dotNL > 0.0f) {
			// Diffuse contribution
			diffuse += dotNL * lightColor;

			// Specular
			if (Shininess > 0.0f && lights[i].direction.w == 1.0) {
				float3 viewVec = normalize(-input.WorldPosition); // Assuming camera at origin
				float3 H = normalize(lightVec + viewVec);
				float dotNH = max(dot(input.Normal, H), 0.0f);
				float spec = pow(dotNH, Shininess);
				specular += spec * lightColor;
			}
		}
	}

	float4 Color = unpackColor(ColorRaw);
	float3 finalColor = saturate(diffuse * Color.rgb + specular);
	if (UseTexture != 0) {
		float4 texel = Texture.Sample(Sampler, input.TexCoord);
		finalColor = saturate(texel.rgb * finalColor);
		output.Color = float4(finalColor, texel.a);
	}
	else {
		output.Color = float4(finalColor, Color.a);
	}
	output.Depth = input.Position.w;
	return output;
}
