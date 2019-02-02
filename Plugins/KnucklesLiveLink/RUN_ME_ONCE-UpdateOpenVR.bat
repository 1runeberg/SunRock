
@ECHO OFF
setlocal enabledelayedexpansion

rem Set key variables
set ue_loc=Program Files\Epic Games
set ue_ver=4.21
set openvr_ver=1_2_10

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

rem Check if latest OpenVR is already installed
if exist "%ue_dir%\Source\ThirdParty\OpenVR\OpenVRv%openvr_ver%" (
    echo [INFO] Latest OpenVR already installed. Nothing to do.
    goto end
)

rem Install latest OpenVR 
echo [INFO] OpenVR version %openvr_ver% not found. Installation started.
echo [INFO] Adding latest OpenVR headers and libraries...
xcopy "%cd%\Misc\OpenVR\*.*" "%ue_dir%\Source\ThirdParty\OpenVR\OpenVRv%openvr_ver%\" /C /S /D /Y /I

echo _
echo [INFO] Adding latest OpenVR binaries to the Engine...
xcopy "%cd%\Misc\OpenVRBin\*.*" "%ue_dir%\Binaries\ThirdParty\OpenVR\OpenVRv%openvr_ver%\" /C /S /D /Y /I

echo _
echo [INFO] Updating OpenVR.build.cs to use latest OpenVR 
xcopy "%cd%\Misc\OpenVR.build.cs" "%ue_dir%\Source\ThirdParty\OpenVR\" /C /D /Y /R

echo _
echo [INFO] OpenVR Updated.

:end
