@echo off
setlocal

:: Set the URL of the ZIP file
set "zip_url=https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2502/dxc_2025_02_20.zip"

:: Set the destination directory where the ZIP will be downloaded and extracted
set "destination_dir=./"

:: Download the ZIP file
echo Downloading ZIP file...
powershell -Command "Invoke-WebRequest -Uri '%zip_url%' -OutFile 'file.zip'"

:: Check if the download was successful
if not exist "file.zip" (
    echo Failed to download the ZIP file.
    exit /b 1
)

:: Extract the ZIP file
echo Extracting ZIP file...
powershell -Command "Expand-Archive -Path 'file.zip' -DestinationPath '%destination_dir%' -Force"

:: Check if the extraction was successful
if errorlevel 1 (
    echo Failed to extract the ZIP file.
    exit /b 1
)

:: Delete the ZIP file after extraction
echo Deleting ZIP file...
del "file.zip"

echo Operation completed successfully.
endlocal