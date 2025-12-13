# Deferred Rendering Pipeline

Compute shader-based deferred lighting with PBR and IBL.

## Overview

TimeToDX uses a deferred rendering pipeline where geometry information is first rendered to multiple render targets (G-Buffer), then lighting is calculated in a single compute shader pass.

## Pipeline Stages

```
Frame

 1. ZPrePass        Depth-only pass
      Output: Depth buffer (DSV)

 2. GBuffer         Geometry pass
      Output: Albedo, Normal, Position, MetallicRoughness, ObjectID

 3. CubeMapPass     IBL generation (once or on demand)
      Output: Irradiance, Prefiltered, BRDF LUT

 4. DeferredPass    Lighting (compute shader)
      Input: G-Buffer + IBL + Lights
      Output: Final color (UAV)

 5. FSQuad          Fullscreen blit

 6. Present
```

## G-Buffer Layout

| RT | Format | Contents | Usage |
|----|--------|----------|-------|
| 0 | RGBA8_UNORM | Albedo RGB, Alpha | Base color |
| 1 | RGBA8_UNORM | Metallic (B), Roughness (G) | PBR params |
| 2 | RGBA16_FLOAT | World Normal XYZ | Lighting |
| 3 | RGBA32_FLOAT | World Position XYZ | Lighting |
| 4 | R32_UINT | Object ID | Picking |

## ZPrePass

Renders depth-only with alpha testing for early-z optimization.

```hlsl
// ZPrePass.hlsl
float4 PS(VS_OUTPUT input) : SV_Target {
    float alpha = albedoTexture.Sample(samplerState, input.texCoord).a;
    if (alpha < 0.5f) discard;
    return float4(0, 0, 0, 1);
}
```

GBuffer pass uses `D3D11_COMPARISON_EQUAL` to skip already-failed pixels.

## GBuffer Pass

Fills all geometry information without lighting calculations.

### Vertex Shader Output
```hlsl
struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};
```

### Pixel Shader Output
```hlsl
struct PS_OUTPUT {
    float4 albedo : SV_TARGET0;
    float2 metallicRoughness : SV_TARGET1;
    float3 normal : SV_TARGET2;
    float4 position : SV_TARGET3;
    uint objectID : SV_TARGET4;
};
```

## Deferred Lighting Pass

Compute shader processes each pixel independently.

### Thread Configuration
```hlsl
[numthreads(16, 16, 1)]
void CS(uint3 DTid : SV_DispatchThreadID) {
    // Process pixel at DTid.xy
}
```

### Input Resources
```hlsl
// G-Buffer
Texture2D<float4> gAlbedo : register(t0);
Texture2D<float2> gMetallicRoughness : register(t1);
Texture2D<float4> gNormal : register(t2);
Texture2D<float4> gPosition : register(t3);

// IBL
TextureCube<float4> irradianceMap : register(t4);
TextureCube<float4> prefilteredMap : register(t5);
Texture2D<float2> brdfLUT : register(t6);

// Lights
StructuredBuffer<LightData> lights : register(t7);
```

### Output
```hlsl
RWTexture2D<float4> outputTexture : register(u0);
```

### PBR Lighting Model
1. **Direct Lighting** - For each light:
   - Cook-Torrance specular BRDF
   - Lambertian diffuse
   - Attenuation (point/spot lights)

2. **Indirect Lighting (IBL)**:
   - Diffuse: Irradiance map sample  albedo
   - Specular: Prefiltered map + BRDF LUT (split-sum)

3. **Tonemapping**: AgX or similar HDRLDR

## IBL Generation (CubeMapPass)

### Resources Generated

| Map | Size | Mips | Purpose |
|-----|------|------|---------|
| Cubemap | 512 | Yes | Environment reflection |
| Irradiance | 32 | No | Diffuse IBL |
| Prefiltered | 128 | 5 | Specular IBL by roughness |
| BRDF LUT | 512 | No | Split-sum lookup |

### Generation Shaders
- `equirectToCubemp.hlsl` - HDR panorama  cubemap
- `irradianceConvolution.hlsl` - Diffuse convolution
- `prefilteredMap.hlsl` - Specular with roughness mips
- `LUT4BRDF.hlsl` - BRDF integration lookup

## Light Data Structure

```hlsl
struct LightData {
    int type;           // 0=Point, 1=Directional, 2=Spot
    float3 position;
    float intensity;
    float3 color;
    float3 direction;
    float2 spotParams;  // Inner/outer cone
    float3 attenuation; // Constant, linear, quadratic
    float radius;
};
```

## Performance Considerations

- **Early-Z**: ZPrePass enables early depth rejection
- **Compute Shader**: Better occupancy than pixel shader for lighting
- **Tiled Deferred**: Could be added for many lights (not implemented)
- **Screen-Space**: All lighting is screen-space, no geometry re-render
