
Texture2D albedoTexture : register(t0);
Texture2D metallicRoughnessTexture : register(t1);
Texture2D normalTexture : register(t2);



[numthreads(8, 8, 6)]
void CS(uint3 tid : SV_DispatchThreadID)
{
    float2 uv = (float2(tid.xy) + 0.5) / float2(w, h);
}
