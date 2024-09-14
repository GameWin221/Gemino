# Building Gemino
## Prerequisites
You have to get the Vulkan SDK from https://vulkan.lunarg.com/ and have it installed. CMake and Visual Studio will find it automatically only when the `VULKAN_SDK` environmental variable is set correctly.
You will also need Vulkan drivers 1.2 or newer.

## Build Tool
### 1. Using CMake
- `mkdir build`
- `cd build`
- `cmake -DCMAKE_BUILD_TYPE=Debug` or `cmake -DCMAKE_BUILD_TYPE=Release`
- Use the specified generator (e.g. make, visual studio, ninja, etc...)

### 2. Using CLion and other JetBrains IDEs
Open the project's directory, your IDE will handle it and configure CMake accordingly. You can use your own configurations.

### 3. Using Visual Studio
There is a .sln file in the project root folder to make Visual Studio users' lives easier. It should work identically as the CMake project.

# Compiling Shaders
Place all of your shaders in the `res/shared/shaders` directory. **Only there** the shaders will get automatically compiled and copied correctly.

### 1. Using CMake or Visual Studio
- CMake will generate custom shader build commands everytime you reload or create the CMake project (`cmake` command). In debug mode it will compile the shaders with additional debug info.

- Visual Studio simply runs `compile.bat` in Release mode and `compile_debug.bat` in Debug mode. It is located in the `res/shaders` directory so the commands don't need to be regenerated.

The shader build commands are executed always after a successful build.

### 2. Manually
Use GLSL to SPIR-V compiler like `glslangValidator` or `glslc` on each shader source file.

On Windows you can run the `compile.bat` script located in the `res/shaders` directory.

## Automatic `res/shared` directory copying
The whole `res/shared` directory will get automatically copied to the build directory after a successful build. (Both using CMake and Visual Studio)
