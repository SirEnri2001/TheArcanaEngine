# DXC Configuration Related
USE "DXC_downloader" to download the required libraries and executables.

# HLSL to SPIR-V using command-line 
```batch
dxc.exe -spirv -T <target-prfile> -E <entry-point>
                 <hlsl-src-file> -Fo <spirv-bin-file>
```