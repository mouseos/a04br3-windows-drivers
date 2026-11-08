// Real CB0 vertex transform; compile vs_4_0. No pretransformed vertex fixture.
cbuffer Transform : register(b0) { float4 row0; float4 row1; float4 row2; float4 row3; };
float4 main(float4 position : POSITION) : SV_Position {
    return float4(dot(position,row0),dot(position,row1),dot(position,row2),dot(position,row3));
}
