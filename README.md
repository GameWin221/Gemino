# Gemino
Designed to be a high performance gpu-driven rendering engine with dynamic applications in mind (such as games).

It's also my passion project for learning and experimenting with new graphics programming concepts.

## Goals
[Milanote Board](https://app.milanote.com/1ReR6Z14XbZ991?p=8MqaOe9BQos)

- Fully "bindless" and GPU driven - Minimal CPU overhead
- Good performance for open worlds and indoor scenes
- Support for most desktop devices

## Implemented Features
- Indirect draw with compute shader draw call generation (vkCmdDrawInstancedIndirectCount)
- Compute shader frustum culling
- Compute shader LOD system
- Handle and Range based resource management
- Transforms based on `vec3, quat, vec3` instead of `mat4` to save memory and lower computation time
- Per-frame Host to Device upload buffers designed to work well with frames in flight 
- Compatible with all Vulkan 1.2 devices
- Simple parent<->children scene graph and scene management
- GLTF Import

# Build Instructions
[See BUILD.md](BUILD.md)

# Showcase
Early tests of Two-Pass Occlusion Culling and LODs:

Exactly 16'000'000 (400 x 100 x 400) monkeys and spheres.
![EarlyGPUOcclusionCullingTests](https://github.com/user-attachments/assets/ee851dfb-f828-41e5-a9b8-d6a64dc7fcc9)

(LODs don't work now because of the improvements made to the scenegraph mesh<->primitive relations. Occlusion culling is removed for now since this particular implementationt brings huge benefits only for little meshes so I'll bring it back if I implement Mesh Shading and understand it more)
