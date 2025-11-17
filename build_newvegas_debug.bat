@echo off

SET "PROJECT=NewVegasReloaded"
SET "FOLDER=nvse"
SET "GAME=NewVegas"
SET "CONFIG=Debug"

@REM Build project
msbuild -target:%PROJECT% /property:Configuration=%CONFIG% /property:Platform=x86 -m

if %ERRORLEVEL% NEQ 0 (
    echo Build has failed. Deploy aborted.
    PAUSE
    exit /b %ERRORLEVEL%
)

@REM Cleanup in case of previous build
rmdir "build" /s /q

@REM Copy dll & pdb FROM THE SAME CONFIG
xcopy "%PROJECT%\%CONFIG%\%PROJECT%.dll" "build\%PROJECT%\%FOLDER%\Plugins\" /y /d
xcopy "%PROJECT%\%CONFIG%\%PROJECT%.pdb" "build\%PROJECT%\%FOLDER%\Plugins\" /y /d

@REM Copy config
xcopy "resource\%PROJECT%.dll.config" "build\%PROJECT%\%FOLDER%\Plugins\" /y /d
xcopy "resource\%PROJECT%.dll.defaults.toml" "build\%PROJECT%\%FOLDER%\Plugins\" /y /d
echo F|xcopy "resource\%PROJECT%.dll.defaults.toml" "build\%PROJECT%\%FOLDER%\Plugins\%PROJECT%.dll.toml" /y /d

@REM Copy Shaders
robocopy /mir .\src\hlsl\%GAME%\Shaders .\build\%PROJECT%\Shaders\%PROJECT%\Shaders
robocopy /mir .\src\hlsl\%GAME%\Effects .\build\%PROJECT%\Shaders\%PROJECT%\Effects

@REM Copy Textures
robocopy /mir .\resource\Textures\ .\build\%PROJECT%\Textures\

echo "No deploy stage."
PAUSE

@REM handle Robocopy weird error codes
if (%ERRORLEVEL% LSS 8) exit /b 0
exit /b %ERRORLEVEL%
