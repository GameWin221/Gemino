# Gemino
TODO
## Motivation
TODO
## Goals
TODO
## Building Project
### 1. Using CMake
- `mkdir build`
- `cd build`
- `cmake -DCMAKE_BUILD_TYPE=Debug` or `cmake -DCMAKE_BUILD_TYPE=Release` (you can specify your own generator)
- Use the specified generator (e.g. make, msbuild, nmake...)

### 2. Using Visual Studio
There is a .sln file in the project root folder to make Visual Studio users' lives easier. It should work identically as the CMake project.

## Compiling Shaders
Place all of your shaders in the `res/shaders` directory. **Only there** the shaders will get automatically compiled and copied correctly.

### 1. Using CMake or Visual Studio
- CMake will generate custom shader build commands everytime you reload or create the CMake project (`cmake` command).

- Visual Studio simply runs the `compile.bat` script located in the `res/shaders` directory so the commands don't need to be regenerated.

The shader build commands are executed always after a successful build.

### 2. Manually
Use GLSL to SPIR-V compiler like `glslangValidator` or `glslc` on each shader source file.

On Windows you can run the `compile.bat` script located in the `res/shaders` directory.

## Automatic `res/` directory copying
The whole `res/` directory will get automatically copied to the build directory after a successful build. (Both using CMake and Visual Studio)