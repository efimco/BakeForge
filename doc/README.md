# TimeToDX - DirectX 11 PBR Renderer

A physically-based rendering engine built with DirectX 11, featuring deferred rendering, image-based lighting, and an ImGui-based editor.

## Features

- **Deferred Rendering** with G-Buffer
- **PBR Materials** with metallic/roughness workflow
- **Image-Based Lighting (IBL)** using HDR environment maps
- **GLTF/GLB Model Loading**
- **Interactive Scene Graph** with drag-and-drop
- **ImGuizmo** for 3D transform manipulation
- **GPU-Based Object Picking**
- **Hot-Reloading Shaders**

## Build System

### Requirements
- C++20 compiler
- CMake 3.10+
- Windows SDK (DirectX 11)

### Dependencies

| Library | Purpose |
|---------|---------|
| **GLM** | Math library (vectors, matrices) |
| **ImGui** | Immediate mode GUI |
| **ImGuizmo** | 3D transform gizmos |
| **tiny_gltf** | GLTF/GLB loading |
| **stb_image** | Image loading (PNG, JPG, HDR) |
| **nlohmann/json** | JSON parsing |

### Build Commands
```
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## Architecture Overview

```

                      Application                            
  main.cpp  Window  Renderer  UIManager                   

                    Render Pipeline                          
  ZPrePass  GBuffer  CubeMapPass  DeferredPass  FSQuad   

                     Scene Graph                             
  Scene  Primitive (mesh)                                 
          Light                                            
          Camera                                           

                    Resources                                
  ShaderManager  Texture  Material  GLTFImporter          

```

## Rendering Pipeline

### Pass Order
1. **ZPrePass** - Depth pre-pass with alpha testing
2. **GBuffer** - Fills geometry buffer textures
3. **CubeMapPass** - Renders environment/skybox
4. **DeferredPass** - Compute shader lighting
5. **FSQuad** - Final blit to screen
6. **UIManager** - ImGui overlay
7. **ObjectPicker** - GPU selection

### G-Buffer Layout

| Texture | Format | Contents |
|---------|--------|----------|
| RT0 | RGBA8 | Albedo (base color) |
| RT1 | RGBA8 | Metallic (B), Roughness (G) |
| RT2 | RGBA16F | World-space normals |
| RT3 | RGBA32F | World-space position |
| RT4 | R32_UINT | Object ID (for picking) |

## Scene Graph

### Class Hierarchy
```cpp
SceneNode (base)
 Primitive   // Renderable mesh
 Light       // Point, Directional, Spot
 Camera      // Orbit camera
 Scene       // Root container
```

### Transform System
```cpp
struct Transform {
    glm::vec3 position;   // Translation
    glm::vec3 rotation;   // Euler angles (degrees)
    glm::vec3 scale;      // Non-uniform scale
    glm::mat4 matrix;     // Computed TRS matrix
};
```

## Camera Controls

| Input | Action |
|-------|--------|
| Middle Mouse | Orbit around pivot |
| Middle Mouse + Shift | Pan |
| Mouse Wheel | Zoom |
| G | Translate mode |
| R | Rotate mode |
| S | Scale mode |

## Key Files

### Core
| File | Description |
|------|-------------|
| `main.cpp` | Entry point, main loop |
| `renderer.cpp` | Render pipeline orchestration |
| `dxDevice.cpp` | D3D11 device initialization |
| `window.cpp` | Win32 window creation |

### Scene
| File | Description |
|------|-------------|
| `scene.cpp` | Scene container, resource management |
| `sceneNode.cpp` | Scene graph node, parenting |
| `primitive.cpp` | Mesh with vertex/index buffers |
| `camera.cpp` | Orbit camera implementation |
| `light.cpp` | Light sources |

### Render Passes
| File | Description |
|------|-------------|
| `passes/ZPrePass.cpp` | Depth pre-pass |
| `passes/GBuffer.cpp` | G-Buffer generation |
| `passes/deferedPass.cpp` | Deferred lighting (compute) |
| `passes/pbrCubeMapPass.cpp` | IBL map generation |
| `passes/FSQuad.cpp` | Fullscreen quad blit |
| `passes/objectPicker.cpp` | GPU object picking |

### Resources
| File | Description |
|------|-------------|
| `shaderManager.cpp` | Shader compilation, hot-reload |
| `texture.cpp` | Texture loading (LDR/HDR) |
| `gltfImporter.cpp` | GLTF/GLB parsing |

### Shaders (`src/shaders/`)
| Shader | Purpose |
|--------|---------|
| `ZPrePass.hlsl` | Depth-only pass |
| `gBuffer.hlsl` | G-Buffer fill |
| `deferred.hlsl` | PBR lighting (compute) |
| `cubemap.hlsl` | Skybox rendering |
| `equirectToCubemp.hlsl` | HDR  Cubemap |
| `irradianceConvolution.hlsl` | Diffuse IBL |
| `prefilteredMap.hlsl` | Specular IBL |
| `LUT4BRDF.hlsl` | BRDF LUT generation |
| `colorPicker.hlsl` | Object picking |

## UI Panels

- **Viewport** - 3D scene with gizmos
- **Scene Graph** - Hierarchical tree view
- **Properties** - Transform, material, light editing
- **Material Browser** - Material thumbnails
- **GBuffer** - Debug visualization
- **Main Menu** - File, Edit, View, Add, Render, Help

## PBR Materials

```cpp
struct Material {
    std::string name;
    std::shared_ptr<Texture> albedo;
    glm::vec4 albedoColor;
    std::shared_ptr<Texture> metallicRoughness;
    float metallicValue;
    float roughnessValue;
    std::shared_ptr<Texture> normal;
};
```

## Lighting

### Light Types
- **Point Light** - Omnidirectional with attenuation
- **Directional Light** - Infinite distance, no falloff
- **Spot Light** - Cone with inner/outer angles

### IBL (Image-Based Lighting)
- Irradiance map for diffuse
- Prefiltered environment map for specular
- BRDF LUT for split-sum approximation
