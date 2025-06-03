struct VS_Input
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL0;
    float4 Color     : COLOR0;
    uint   TexId     : TEXCOORD0;
    float2 TexCoord  : TEXCOORD1;
    float  Shininess : TEXCOORD2;
};

struct FS_Input
{
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL0;
    float4 Color         : COLOR0;
    uint   TexId         : TEXCOORD0;
    float2 TexCoord      : TEXCOORD1;
    float Shininess      : TEXCOORD2;
    float3 WorldPosition : TEXCOORD3;
};

struct FS_Output
{
    float4 Color : SV_Target0;
    float  Depth : SV_Depth;
};

struct SceneLight {
    float4 color;
    float4 position;
    float4 direction;
};
