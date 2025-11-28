@echo off
echo ============================================
echo GLSL → SPIR-V Compiler
echo ============================================

if "%~1"=="" (
    echo Usage: compile_spirv.bat shaderfile.glsl
    echo Example: compile_spirv.bat Desaturate.comp.glsl
    pause
    exit /b 1
)

set GLSL=%~1
set SPV=%~1.spv

echo.
echo Compiling: %GLSL%
echo Output   : %SPV%
echo.

glslangValidator.exe -V "%GLSL%" -o "%SPV%"

if %errorlevel% neq 0 (
    echo.
    echo *** Compilation FAILED ***
    pause
    exit /b 1
)

echo.
echo *** Compilation SUCCESS ***
pause
