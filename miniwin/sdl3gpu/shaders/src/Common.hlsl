struct VS_Input
{
    float3 Position : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float4 Color : TEXCOORD2;
};

struct FS_Input
{
    float4 Color : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float4 Position : SV_Position;
};

struct FS_Output
{
    float4 Color : SV_Target0;
    float Depth : SV_Depth;
};
