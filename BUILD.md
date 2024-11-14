# Building Gemino
## Prerequisites
You have to get the Vulkan SDK from https://vulkan.lunarg.com/ and have it installed. CMake will find it automatically only when the `VULKAN_SDK` environmental variable is set correctly.
You will also need Vulkan drivers 1.2 or newer.

Sponza scene is required for the default example and can be downloaded [here](https://www.intel.com/content/www/us/en/developer/topic-technology/graphics-research/samples.html), from the Intel GPU Research samples. After you download it, provide the path to it in the main.cpp file.

## Build Tool
### 1. Using CMake
- `mkdir build`
- `cd build`
- `cmake -DCMAKE_BUILD_TYPE=Debug` or `cmake -DCMAKE_BUILD_TYPE=Release`
- Use the specified generator (e.g. make, visual studio, ninja, etc...)

### 2. Using CLion and other JetBrains IDEs
Open the project's directory, your IDE will handle it and configure CMake accordingly. You can use your own configurations.

# Compiling Shaders
Place all of your shaders in the `src/renderer/shaders` directory. **Only there** the shaders will get automatically compiled and copied correctly.

### 1. Using CMake
CMake will generate custom shader *build-and-copy* commands for each GLSL source file everytime you reload or create the CMake project (`cmake` command).

The commands are always executed during the build process only for the sources that changed compared to the last compiled version (Or if a re-build was issued). Each compiled `.spv` file will get copied into the `shaders/` directory located in the target build directory while maintaining the file structure (e.g. shaders in subdirectories).

### 2. Manually (Not recommended)
Use GLSL to SPIR-V compiler like `glslangValidator` or `glslc` on each shader source file.

On Windows you can run the `compile.bat` script located in the `src/renderer/shaders` directory.

Copy the resulting `.spv` files manually to the `shaders/` directory located in the target build directory.