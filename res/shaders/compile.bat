%VULKAN_SDK%\Bin\glslc.exe %~dp0\default.frag -o %~dp0\default.frag.spv
%VULKAN_SDK%\Bin\glslc.exe %~dp0\default.vert -o %~dp0\default.vert.spv
%VULKAN_SDK%\Bin\glslc.exe %~dp0\default.comp -o %~dp0\default.comp.spv

pause