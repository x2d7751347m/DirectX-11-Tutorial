struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

VS_OUTPUT VS(float4 inPos : POSITION, float4 inColor : COLOR, float4 inColorTwo : COLORTWO)
{
    VS_OUTPUT output;

    output.Pos = inPos;
    output.Color.r = inColor.r * inColorTwo.r;
    output.Color.g = inColor.g * inColorTwo.g;
    output.Color.b = inColor.b * inColorTwo.b;
    output.Color.a = inColor.a * inColorTwo.a;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    return input.Color;
}