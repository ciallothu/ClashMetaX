@echo off
setlocal EnableDelayedExpansion
echo ========================================
echo SysProxyBar Build Script
echo ========================================
echo.

REM ========================================
REM Step 1: Read version from CMakeLists.txt
REM ========================================
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

REM ========================================
REM Step 2: Detect TDM-GCC
REM ========================================
set "TDM_GCC=D:\env\Scoop\apps\tdm-gcc\10.3.0\bin"

if not exist "!TDM_GCC!\gcc.exe" (
    echo ERROR: TDM-GCC not found at !TDM_GCC!
    echo Please install: scoop install tdm-gcc
    pause
    exit /b 1
)
echo Found TDM-GCC
echo.

REM ========================================
REM Step 3: Generate WebUI resources
REM ========================================
echo Generating WebUI resources...
pixi run python generate_resources.py
if errorlevel 1 (
    echo Failed to generate resources!
    pause
    exit /b 1
)
echo.

REM ========================================
REM Step 4: Build project
REM ========================================
echo Building...

REM Use forward slashes for CMake paths
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

REM ========================================
REM Step 5: Verify output
REM ========================================
set "OUTPUT_DIR=release\v!VERSION!"

if exist "!OUTPUT_DIR!\SysProxyBar.exe" (
    echo.
    echo ========================================
    echo SUCCESS!
    echo ========================================
    echo Version: !VERSION!
    echo Output: !OUTPUT_DIR!\SysProxyBar.exe
    dir "!OUTPUT_DIR!\SysProxyBar.exe" | findstr "SysProxyBar.exe"
    echo ========================================
) else (
    echo ERROR: Output not found at !OUTPUT_DIR!\SysProxyBar.exe
)

echo.

endlocal
pause
