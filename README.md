# BakeForge

An open source GPU-accelerated texture baker built with DirectX 11. Bake high-poly detail onto low-poly meshes with real-time preview and physically-based rendering.

![DirectX 11](https://img.shields.io/badge/DirectX-11-blue)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)
![License](https://img.shields.io/badge/License-MIT-green)

## Features

### GPU Baking
- **Normal Map Baking** - Transfer high-poly surface detail to tangent-space normal maps
- **BVH-Accelerated Ray Tracing** - Fast GPU raycast using Bounding Volume Hierarchy
- **Cage-Based Projection** - Adjustable cage offset for controlling ray distance
- **Smoothed Normals Option** - Toggle between faceted and smoothed normal projection
- **Multi-Material Support** - Bake separate maps per material automatically
- **Configurable Resolution** - Output textures from 2x2 up to 4096x4096

### Real-Time Preview
- **PBR Viewport** - See baked results with physically-based rendering
- **Image-Based Lighting** - HDR environment maps with adjustable intensity
- **Deferred Rendering** - Efficient multi-light preview scenes
- **BVH Debug Visualization** - Inspect bounding volume hierarchy structure

### Scene Management
- **glTF Import** - Load high-poly and low-poly meshes from glTF/GLB files
- **Hierarchical Scene Graph** - Organize bake groups with drag-and-drop
- **Material Browser** - Visual material assignment and editing
- **Undo/Redo System** - Full command history with transaction support

## Screenshots

![alt text](image-1.png)

3D Asset is Done by [Igor Oskolskiy](https://www.artstation.com/dark_igorek)

## Quick Start

### 1. Import Your Meshes
- `File → Import Model` to load your low-poly and high-poly meshes

### 2. Create a Baker Node
- `Add → Baker` to create a new bake group
- Drag your **low-poly** mesh under the `[LP]` node
- Drag your **high-poly** mesh under the `[HP]` node

### 3. Configure Bake Settings
- Select the Baker node to access settings in the Properties panel
- Adjust **Cage Offset** to control projection distance
- Set **Texture Size** for output resolution
- Choose output **Directory** and **Filename**

### 4. Bake!
- Click **Start Bake** to generate your normal map
- Results are saved to the specified path


![alt text](BakeExampleDoor.png)

## Requirements

- Windows 10/11
- DirectX 11 compatible GPU
- Visual Studio 2022 (C++20 support)
- CMake 3.10+

## Building

```bash
# Clone the repository
git clone https://github.com/efimco/BakeForge.git
cd BakeForge

# Configure with CMake
cmake --preset=default

# Build
cmake --build build --config Release

# Run
./build/Release/TimeToDX.exe
```

Or open `build/TimeToDX.sln` in Visual Studio and build from there.

## Baking Pipeline

```
┌──────────────────┐
│   UV Rasterize   │  Rasterize low-poly UVs to get texel positions/normals
└────────┬─────────┘
         ▼
┌──────────────────┐
│   Build BVH      │  Construct acceleration structure for high-poly
└────────┬─────────┘
         ▼
┌──────────────────┐
│   GPU Raycast    │  Cast rays from low-poly surface toward high-poly
└────────┬─────────┘
         ▼
┌──────────────────┐
│  Normal Transfer │  Sample high-poly normals at hit points
└────────┬─────────┘
         ▼
┌──────────────────┐
│  Tangent Space   │  Convert world normals to tangent-space normal map
└────────┬─────────┘
         ▼
┌──────────────────┐
│   Export PNG     │  Save final texture to disk
└──────────────────┘
```

## Controls

| Input | Action |
|-------|--------|
| **MMB** | Orbit camera |
| **Shift + MMB** | Pan around |
| **F** | Focus On Object |
| **Scroll** | Zoom |
| **LMB** | Select object |
| **G / R / S** | Translate / Rotate / Scale gizmo |
| **M** | Toggle snapping |
| **Ctrl+Z** | Undo |
| **Ctrl+Shift+Z** | Redo |
| **Shift+D** | Duplicate selected |
| **Delete** | Delete selected |

## Roadmap

- [ ] Ambient Occlusion baking
- [ ] Multi-threaded BVH construction
- [ ] Batch baking multiple objects
- [ ] Anti-aliased edge sampling
- [ ] Vulkan/DX12 backend

## Dependencies

All dependencies are included in `thirdparty/Include/`:

- [GLM](https://github.com/g-truc/glm) - Mathematics library
- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) - 3D gizmos
- [tinygltf](https://github.com/syoyo/tinygltf) - glTF 2.0 loader
- [DirectXTex](https://github.com/microsoft/DirectXTex) - Texture processing library

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- HDR environment maps from [Poly Haven](https://polyhaven.com/)
- Inspired by [Marmoset Toolbag](https://marmoset.co/toolbag/) and [xNormal](https://xnormal.net/)
- Test models from Igor Oskolskiy
