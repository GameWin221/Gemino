# Gemino
Designed to be a high performance gpu-driven rendering framework with real-world applications in mind (e.g. games).

It's also my passion project for learning and experimenting with new graphics programming concepts.

## Goals
[Milanote Board](https://app.milanote.com/1ReR6Z14XbZ991?p=8MqaOe9BQos)

- Fully "bindless" and GPU driven - Minimal CPU overhead
- Dynamic scenes and resources with many updates per frame
- Good performance for open worlds and indoor scenes
- Support for most desktop devices
- Friendly interface that also doesn't take away most of the control

## Major Features
- Indirect draw with compute shader draw call generation (vkCmdDrawInstancedIndirectCount)
- Two-pass occlusion culling
- Compute shader frustum culling
- Compute shader LOD system
- Handle based **Resource Managers**
- Compatible with all Vulkan 1.2 devices

# Build Instructions
[See BUILD.md](BUILD.md)

# Showcase
First GPU Occlusion Culling Implementation
![EarlyGPUOcclusionCullingTests](https://github.com/GameWin221/Gemino/assets/72656547/4c6ca538-9987-4b50-a3dd-cff9f9aab1b6)
About 70fps when all monkeys are visible, about 170fps when looking from close up.

Some cool visuals will be gradually added here...

