/*
The MIT License(MIT)

Copyright(c) 2018 runeberg.io
Contact via Twitter: @1runeberg

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files(the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "SteamVRInputDeviceFunctionLibrary.h"

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
#include "openvr.h"
#include "HAL/FileManagerGeneric.h"
using namespace vr;
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

void USteamVRInputDeviceFunctionLibrary::PlaySteamVR_HapticFeedback(bool VibrateLeft, float StartSecondsFromNow, float DurationSeconds, float Frequency, float Amplitude /*= 0.5f*/)
{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	EVRInitError SteamVRInitError = VRInitError_None;
	IVRSystem* SteamVRSystem = VR_Init(&SteamVRInitError, VRApplication_Scene);

	if (SteamVRInitError == VRInitError_None)
	{
		const FString ManifestPath = FPaths::GeneratedConfigDir() / ACTION_MANIFEST;
		EVRInputError Err = VRInput()->SetActionManifestPath(TCHAR_TO_ANSI(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ManifestPath)));
		
		if (Err == VRInputError_None)
		{
			VRActionHandle_t vrKnucklesVibrationLeft;
			VRActionHandle_t vrKnucklesVibrationRight;

			if (Amplitude < 0.f)
			{
				Amplitude = 0.f;
			}
			else if (Amplitude > 1.f)
			{
				Amplitude = 1.f;
			}

			if (VibrateLeft)
			{
				Err = VRInput()->GetActionHandle(TCHAR_TO_ANSI(*FString(TEXT(ACTION_PATH_VIBRATE_LEFT))), &vrKnucklesVibrationLeft);
				Err = VRInput()->TriggerHapticVibrationAction(vrKnucklesVibrationLeft, StartSecondsFromNow,
					DurationSeconds, Frequency, Amplitude, k_ulInvalidInputValueHandle);
			}
			else
			{
				Err = VRInput()->GetActionHandle(TCHAR_TO_ANSI(*FString(TEXT(ACTION_PATH_VIBRATE_RIGHT))), &vrKnucklesVibrationRight);
				Err = VRInput()->TriggerHapticVibrationAction(vrKnucklesVibrationRight, StartSecondsFromNow,
					DurationSeconds, Frequency, Amplitude, k_ulInvalidInputValueHandle);
			}
		}
	}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
}
