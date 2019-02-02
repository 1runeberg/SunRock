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

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "openvr.h"
#include "KnucklesLiveLinkSource.h"
#include "ILiveLinkClient.h"
#include "KnucklesLiveLinkRuntime.generated.h"

using namespace vr;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class KNUCKLESLIVELINK_API UKnucklesLiveLinkRuntime : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UKnucklesLiveLinkRuntime();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	UFUNCTION(BlueprintCallable, Category="Knuckles")
	bool ConnectToKnucklesSource(bool GetKnucklesSkelAnim = true, bool LeftWithController = false, bool RightWithController = false);

	UFUNCTION(BlueprintCallable, Category = "Knuckles")
	void CleanupKnucklesSource();

	UFUNCTION(BlueprintCallable, Category = "Knuckles")
	void PlayKnucklesHapticFeedback(bool VibrateLeft, float StartSecondsFromNow, float DurationSeconds = 1.f, 
		float Frequency = 1.f, float Amplitude = 0.5f);

	/** Whether SteamVR is active or not */
	UPROPERTY(BlueprintReadOnly, Category = "VR")
	bool bSteamVRPresent;

	/** Whether Right Knuckles is present or not */
	UPROPERTY(BlueprintReadOnly, Category = "VR")
	bool bRightKnucklesPresent;

	/** Whether Left Knuckles is present or not */
	UPROPERTY(BlueprintReadOnly, Category = "VR")
	bool bLeftKnucklesPresent;

private:
	void GetInputError(EVRInputError InputError, FString InputAction);

	// SteamVR Input System
	IVRSystem* VRSystem = nullptr;
	VRActionHandle_t vrKnucklesVibrationLeft;
	VRActionHandle_t vrKnucklesVibrationRight;

	// Setup LiveLinkClient
	TSharedPtr<FKnucklesLiveLinkSource> NewSource = nullptr;
	ILiveLinkClient* LiveLinkClient;

	// Knuckles Controller IDs
	int KnucklesControllerIdLeft = -1;
	int KnucklesControllerIdRight = -1;
};
