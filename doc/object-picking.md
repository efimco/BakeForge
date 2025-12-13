# Object Picking System

GPU-based object selection using a color ID buffer.

## Overview

The object picking system renders unique integer IDs to a dedicated render target during the GBuffer pass, then samples at the mouse cursor using a compute shader.

## Architecture

```
1. GBuffer Pass
    Renders object IDs to R32_UINT render target

2. UI Pass
    Captures mouse position relative to viewport

3. ObjectPicker Pass
    Dispatches compute shader at mouse coordinates
    Reads object ID from GBuffer texture
    Copies result to staging buffer
    Maps to CPU and updates scene selection
```

## Components

### Files
| File | Description |
|------|-------------|
| `passes/objectPicker.hpp` | ObjectPicker class declaration |
| `passes/objectPicker.cpp` | ObjectPicker implementation |
| `passes/GBuffer.cpp` | Object ID render target |
| `shaders/colorPicker.hlsl` | Compute shader for ID sampling |

### GPU Buffers

| Buffer | Type | Size | Purpose |
|--------|------|------|---------|
| Constant Buffer | Dynamic CB | 16 bytes | Mouse position |
| Structured Buffer | UAV | 4 bytes | Picked ID on GPU |
| Staging Buffer | Staging | 4 bytes | CPU readback |

## Object ID Encoding

IDs are **1-indexed** based on primitive draw order:

| ID Value | Meaning |
|----------|---------|
| 0 | Background / No object |
| 1 | First primitive (index 0) |
| 2 | Second primitive (index 1) |
| N | Primitive at index N-1 |

## Shader Implementation

### GBuffer Pixel Shader
```hlsl
struct PSOutput {
    float4 albedo : SV_TARGET0;
    float2 metallicRoughness : SV_TARGET1;
    float3 normal : SV_TARGET2;
    float4 fragPos : SV_TARGET3;
    uint objectID : SV_TARGET4;  // Object ID
};

PSOutput PS(VertexOutput input) {
    output.objectID = objectID;  // From constant buffer
    return output;
}
```

### Color Picker Compute Shader
```hlsl
cbuffer cbPickLocation : register(b0) {
    uint mousePosX;
    uint mousePosY;
    uint2 _pad;
}

Texture2D<uint> gBufferObjectID : register(t0);
RWStructuredBuffer<uint> pickedColor : register(u0);

[numthreads(1, 1, 1)]
void CS(uint3 DTid : SV_DISPATCHTHREADID) {
    uint idColor = gBufferObjectID.Load(int3(mousePosX, mousePosY, 0));
    pickedColor[0] = idColor;
}
```

## GPU-to-CPU Readback

1. Update constant buffer with mouse position
2. Bind object ID SRV and output UAV
3. Dispatch single thread `(1,1,1)`
4. Copy structured buffer to staging buffer
5. Map staging buffer and read the ID
6. Update scene selection on mouse click

## Selection Features

| Action | Result |
|--------|--------|
| Click on object | Select (deselects others) |
| Shift + Click | Add to selection |
| Click on background | Clear selection |

## Performance

- **Single thread dispatch** - Minimal GPU cost
- **1-2 frame latency** - Due to staging buffer readback
- **Per-frame execution** - Instant response on click
