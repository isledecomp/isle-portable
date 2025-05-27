#include "Common.hlsl"

FS_Output main(FS_Input input)
{
    FS_Output output;
    output.Color = input.Color;
    output.Depth = input.Position.w;
    return output;
}
