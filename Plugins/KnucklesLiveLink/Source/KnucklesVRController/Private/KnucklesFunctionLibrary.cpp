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

#include "KnucklesFunctionLibrary.h"

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
#include "openvr.h"
#include "ISteamVRPlugin.h"
using namespace vr;
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

void UKnucklesFunctionLibrary::PlaySteamVR_HapticFeedback(bool VibrateLeft, float StartSecondsFromNow, float DurationSeconds, float Frequency, float Amplitude /*= 0.5f*/)
{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	ISteamVRPlugin* SteamVRPlugin;
	SteamVRPlugin = &FModuleManager::LoadModuleChecked<ISteamVRPlugin>(TEXT("SteamVR"));
	IVRSystem* VRSystem = SteamVRPlugin->GetVRSystem();

	if (VRSystem)
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
			VRInput()->GetActionHandle(TCHAR_TO_UTF8(*FString(TEXT("/actions/main/out/VibrateLeft"))), &vrKnucklesVibrationLeft);
			VRInput()->TriggerHapticVibrationAction(vrKnucklesVibrationLeft, StartSecondsFromNow,
				DurationSeconds, Frequency, Amplitude, k_ulInvalidInputValueHandle);
		}
		else
		{
			VRInput()->GetActionHandle(TCHAR_TO_UTF8(*FString(TEXT("/actions/main/out/VibrateRight"))), &vrKnucklesVibrationRight);
			VRInput()->TriggerHapticVibrationAction(vrKnucklesVibrationRight, StartSecondsFromNow,
				DurationSeconds, Frequency, Amplitude, k_ulInvalidInputValueHandle);
		}
	}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
}
