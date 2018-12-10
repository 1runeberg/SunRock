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

#include "KnucklesLiveLinkRuntime.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// Sets default values for this component's properties
UKnucklesLiveLinkRuntime::UKnucklesLiveLinkRuntime()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UKnucklesLiveLinkRuntime::BeginPlay()
{
	Super::BeginPlay();
	
}

bool UKnucklesLiveLinkRuntime::ConnectToKnucklesSource(bool GetKnucklesSkelAnim, bool LeftWithController, bool RightWithController)
{	
	// Initialise
	bSteamVRPresent = false;
	NewSource = MakeShared<FKnucklesLiveLinkSource>();

	if (NewSource)
	{
		// Customize source output
		NewSource->bWithKnucklesAnim = GetKnucklesSkelAnim;
		NewSource->bRangeWithControllerL = LeftWithController;
		NewSource->bRangeWithControllerR = RightWithController;


		if (LiveLinkClient == nullptr)
		{
			IModularFeatures& ModularFeatures = IModularFeatures::Get();
			if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
			{
				LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			}
		}

		if (LiveLinkClient)
		{
			// Add Source to Client
			LiveLinkClient->AddSource(NewSource);

			// Get SteamVRSystem
			if (NewSource->SteamVRSystem)
			{
				VRSystem = NewSource->SteamVRSystem;
			}

			// Get Source State
			bSteamVRPresent = NewSource->bSteamVRPresent;
			bLeftKnucklesPresent = NewSource->bLeftKnucklesPresent;
			bRightKnucklesPresent = NewSource->bRightKnucklesPresent;
		}
	}

	return bSteamVRPresent;
}

void UKnucklesLiveLinkRuntime::CleanupKnucklesSource()
{
	if (LiveLinkClient)
	{
		if (NewSource)
		{
			LiveLinkClient->RemoveSource(NewSource);
			NewSource = nullptr;

		}

		LiveLinkClient = nullptr;
	}
}

void UKnucklesLiveLinkRuntime::PlayKnucklesHapticFeedback(bool VibrateLeft, float StartSecondsFromNow, float DurationSeconds, float Frequency, float Amplitude /*= 0.5f*/)
{
	if (bSteamVRPresent && VRSystem)
	{
		if (Amplitude < 0.f)
		{
			Amplitude = 0.f;
		}
		else if (Amplitude > 1.f)
		{
			Amplitude = 1.f;
		}
		
		if (VibrateLeft && bLeftKnucklesPresent)
		{
			// TODO: Implement in VRKnucklesController instead; use errorhandling there and action manifest
			VRInput()->GetActionHandle(TCHAR_TO_UTF8(*FString(TEXT("/actions/main/out/VibrateLeft"))), &vrKnucklesVibrationLeft);
			VRInput()->TriggerHapticVibrationAction(vrKnucklesVibrationLeft, StartSecondsFromNow, 
				DurationSeconds, Frequency, Amplitude, k_ulInvalidInputValueHandle);
		}
		else if (bRightKnucklesPresent)
		{
			// TODO: Implement in VRKnucklesController instead; use errorhandling there and action manifest
			VRInput()->GetActionHandle(TCHAR_TO_UTF8(*FString(TEXT("/actions/main/out/VibrateRight"))), &vrKnucklesVibrationRight);
			VRInput()->TriggerHapticVibrationAction(vrKnucklesVibrationRight, StartSecondsFromNow, 
				DurationSeconds, Frequency, Amplitude, k_ulInvalidInputValueHandle);
		}
	}
}
