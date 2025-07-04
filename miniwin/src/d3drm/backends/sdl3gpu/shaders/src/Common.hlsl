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
    #define REGISTER_B0_SPACE1 register(b0)
    #define REGISTER_B1_SPACE3 register(b0)
    #define REGISTER_T0_SPACE2 register(t0)
    #define REGISTER_S0_SPACE2 register(s0)
#else
    #define REGISTER_B0_SPACE1 register(b0, space1)
    #define REGISTER_B1_SPACE3 register(b0, space3)
    #define REGISTER_T0_SPACE2 register(t0, space2)
    #define REGISTER_S0_SPACE2 register(s0, space2)
#endif
