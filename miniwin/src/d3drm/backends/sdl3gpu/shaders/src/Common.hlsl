struct VS_Input
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL0;
    float2 TexCoord  : TEXCOORD1;
};

struct FS_Input
{
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL0;
    float2 TexCoord      : TEXCOORD1;
    float3 WorldPosition : TEXCOORD3;
};

struct SceneLight {
    float4 color;
    float4 position;
    float4 direction;
};

#ifdef NO_REGISTER_SPACE
    #define REGISTER_SPACE(REG, SPACE) register(REG)
#else
    #define REGISTER_SPACE(REG, SPACE) register(REG, SPACE)
#endif