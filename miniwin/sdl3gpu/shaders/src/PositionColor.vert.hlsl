struct Input
{
    float3 Position : TEXCOORD0;
    float4 Color : TEXCOORD1;
};

struct Output
{
    float4 Color : TEXCOORD0;
    float4 Position : SV_Position;
};

cbuffer ViewportUniforms : register(b0, space1)
{
    float4x4 perspective;
};

Output main(Input input)
{
    Output output;
    output.Color = input.Color;
    output.Position = mul(perspective, float4(input.Position, 1.0));
    return output;
}
