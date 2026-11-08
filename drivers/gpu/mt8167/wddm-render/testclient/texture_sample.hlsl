Texture2D<float4> image : register(t0);
SamplerState sampling : register(s0);
cbuffer Coordinates : register(b0) { float4 uv; };
float4 main() : SV_Target { return image.Sample(sampling, uv.xy); }
