@echo off
setlocal EnableDelayedExpansion
echo ========================================
echo ClashMetaX Build Script
echo ========================================
echo.

echo Reading version...
for /f "tokens=3" %%V in ('type CMakeLists.txt ^| findstr /C:"project" ^| findstr /C:"VERSION"') do (
    set "VERSION=%%V"
    goto :version_found
)
:version_found

if "!VERSION!"=="" (
    echo ERROR: Failed to read version from CMakeLists.txt
    pause
    exit /b 1
)
echo Version: !VERSION!
echo.

set "TDM_GCC=D:\env\Scoop\apps\tdm-gcc\10.3.0\bin"

if not exist "!TDM_GCC!\gcc.exe" (
    echo ERROR: TDM-GCC not found at !TDM_GCC!
    echo Please install: scoop install tdm-gcc
    pause
    exit /b 1
)
echo Found TDM-GCC
echo.

echo Generating WebUI resources...
pixi run python generate_resources.py
if errorlevel 1 (
    echo Failed to generate resources!
    pause
    exit /b 1
)
echo.

echo Generating icon resources...
pixi run python tools/generate_icons.py
if errorlevel 1 (
    echo Failed to generate icons!
    pause
    exit /b 1
)
echo.

echo Building...
set "GCC_EXE=D:/env/Scoop/apps/tdm-gcc/10.3.0/bin/g++.exe"
set "MAKE_EXE=D:/env/Scoop/apps/tdm-gcc/10.3.0/bin/mingw32-make.exe"
set "WINDRES_EXE=D:/env/Scoop/apps/tdm-gcc/10.3.0/bin/windres.exe"

if not exist build mkdir build
cd build
if exist CMakeCache.txt del /Q CMakeCache.txt

cmake .. -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="%GCC_EXE%" -DCMAKE_MAKE_PROGRAM="%MAKE_EXE%" -DCMAKE_RC_COMPILER="%WINDRES_EXE%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo Configuration FAILED!
    cd ..
    pause
    exit /b 1
)

"%TDM_GCC%\mingw32-make.exe"
if errorlevel 1 (
    echo Build FAILED!
    cd ..
    pause
    exit /b 1
)

cd ..
echo Build complete!
echo.

set "OUTPUT_DIR=release\v!VERSION!"
if exist "!OUTPUT_DIR!\ClashMetaX.exe" (
    echo.
    echo ========================================
    echo SUCCESS!
    echo ========================================
    echo Version: !VERSION!
    echo Output: !OUTPUT_DIR!\ClashMetaX.exe
    dir "!OUTPUT_DIR!\ClashMetaX.exe" | findstr "ClashMetaX.exe"
    echo ========================================
) else (
    echo ERROR: Output not found at !OUTPUT_DIR!\ClashMetaX.exe
)

echo.
endlocal
pause
