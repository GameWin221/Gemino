echo off
echo Starting shader compilation
for %%f in (%~dp0*.vert, %~dp0*.frag, %~dp0*.comp, %~dp0*.tesc, %~dp0*.tese, %~dp0*.geom, %~dp0*.rgen, %~dp0*.rint, %~dp0*.rahit, %~dp0*.rchit, %~dp0*.rmiss, %~dp0*.rcall) do (
    echo Compiling: %~dp0%%f
    %VULKAN_SDK%\Bin\glslc.exe %%f -o %%f.spv
)
echo Finished shader compilation
pause