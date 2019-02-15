
@ECHO OFF
setlocal enabledelayedexpansion

rem Set key variables
set ue_loc=Program Files\Epic Games
set ue_ver=4.21
set ue_dir=""

rem Set valid drives
set drives[0]=C
set drives[1]=D
set drives[2]=E
set drives[3]=F
set drives[4]=G

rem Look for OpenVR in UE4 Launcher Build
echo _
echo [INFO] Looking for Unreal Engine on this PC
for /F "tokens=2 delims==" %%d in ('set drives[') do (
    set ue_dir=%%d:\%ue_loc%\UE_%ue_ver%\Engine
    if exist "!ue_dir!\Source\ThirdParty\OpenVR" (
        goto done_ue_check
    )
    set ue_dir=""
)

rem Check if there's a valid UE diretory
if %ue_dir% == "" (
    echo [ERROR] No valid Unreal Engine Path found.
    goto end
)

:done_ue_check
echo [INFO] Unreal Engine found at: %ue_dir%

echo _
echo [INFO] Resetting OpenVR.build.cs to use stock OpenVR 
xcopy "%cd%\Misc\OpenVR.build.cs" "%ue_dir%\Source\ThirdParty\OpenVR\" /C /D /Y /R

echo _
echo [INFO] OpenVR Updated.

:end
