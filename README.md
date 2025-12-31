# TimeToDX

A real-time 3D renderer built with DirectX 11, featuring physically-based rendering (PBR), image-based lighting (IBL), and a deferred rendering pipeline. Inspired by Marmoset Toolbag.

![DirectX 11](https://img.shields.io/badge/DirectX-11-blue)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

## Features

### Rendering
- **Deferred Rendering Pipeline** - Compute shader-based lighting for efficient multi-light scenes
- **Physically-Based Rendering (PBR)** - Cook-Torrance BRDF with metallic-roughness workflow
- **Image-Based Lighting (IBL)** - HDR environment maps with irradiance and prefiltered specular
- **Z-Prepass** - Early depth testing for overdraw reduction

### Scene Management
- **Hierarchical Scene Graph** - Parent-child transform propagation
- **glTF Import** - Load 3D models with materials and textures
- **Multiple Light Types** - Point, directional, and spot lights
- **Object Picking** - GPU-based selection via object ID buffer

### Tools
- **ImGui Interface** - Scene hierarchy, material editing, light properties
- **Gizmo Manipulation** - Translate, rotate, scale objects
- **Undo/Redo System** - Command pattern with transaction support
- **BVH Visualization** - Debug view of bounding volume hierarchy

## Screenshots
![alt text](image-1.png)

## Requirements

- Windows 10/11
- Visual Studio 2022 (or compatible compiler with C++20 support)
- CMake 3.10+
- DirectX 11 compatible GPU

## Building

### Clone the Repository

```bash
git clone https://github.com/efimco/DOCXEngineDirectX.git
cd DOCXEngineDirectX
```

### Configure with CMake

```bash
mkdir build
cd build
cmake ..
```

Or use CMake presets:

```bash
cmake --preset=default
```

### Build

**Using Visual Studio:**
1. Open `build/TimeToDX.sln`
2. Set `TimeToDX` as the startup project
3. Build and run (F5)

**Using Command Line:**
```bash
cmake --build build --config Release
```

### Run

The executable expects resources to be at `../../res/` relative to the working directory. When running from Visual Studio, the working directory is automatically set.

```bash
cd build/Release
TimeToDX.exe
```

## Render Pipeline

```
┌─────────────┐
│  ZPrePass   │  Depth-only rendering
└──────┬──────┘
       ▼
┌─────────────┐
│   GBuffer   │  Albedo, Normal, Position, Metallic/Roughness, ObjectID
└──────┬──────┘
       ▼
┌─────────────┐
│  CubeMap    │  IBL: Irradiance, Prefiltered env, BRDF LUT
└──────┬──────┘
       ▼
┌─────────────┐
│  Deferred   │  Compute shader lighting (PBR + IBL)
└──────┬──────┘
       ▼
┌─────────────┐
│   FSQuad    │  Fullscreen blit to backbuffer
└──────┬──────┘
       ▼
┌─────────────┐
│    ImGui    │  UI overlay
└─────────────┘
```

## Controls

| Input | Action |
|-------|--------|
| **RMB + WASD** | Fly camera |
| **RMB + Mouse** | Look around |
| **Scroll** | Zoom |
| **LMB** | Select object |
| **W/E/R** | Translate/Rotate/Scale gizmo |
| **Ctrl+Z** | Undo |
| **Ctrl+Y** | Redo |

## Dependencies

All dependencies are included in `thirdparty/Include/`:

- [GLM](https://github.com/g-truc/glm) - Mathematics library
- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) - 3D gizmos
- [tinygltf](https://github.com/syoyo/tinygltf) - glTF 2.0 loader
- [stb_image](https://github.com/nothings/stb) - Image loading

## Documentation

See the [doc/](doc/) folder for detailed documentation:
- [Deferred Rendering](doc/deferred-rendering.md)
- [Scene Graph](doc/scene-graph.md)
- [Object Picking](doc/object-picking.md)

## License

*Add your license here*

## Acknowledgments

- HDR environment maps from [Poly Haven](https://polyhaven.com/)
- Inspired by [Marmoset Toolbag](https://marmoset.co/toolbag/)
