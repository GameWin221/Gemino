# Gemino
Designed to be a high performance gpu-driven rendering framework with real-world applications in mind (e.g. games).
The underlying Gemino API provides an interface to make custom **Render Paths**, manage **Worlds**, interact with the **Input Manager** and import resources. **Renderer** and its **Resource Managers** are abstractions over the Vulkan API that simplify it while still giving the freedom and control to the user if they need it.

It's also my test bench for learning and experimenting with new graphics programming concepts.

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
Some cool visuals will be gradually added here...

