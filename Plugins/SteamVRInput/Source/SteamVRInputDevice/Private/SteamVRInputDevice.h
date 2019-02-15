/*
The MIT License(MIT)

Copyright(c) 2019 runeberg.io
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

#include "IInputDevice.h"
#include "Core.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SteamVRInputTypes.h"
#include "SteamVRKnucklesKeys.h"

class FSteamVRInputDevice : public IInputDevice
{
public:
	FSteamVRInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);
	~FSteamVRInputDevice();

	/** Tick the interface (e.g. check for new controllers) */
	virtual void Tick(float DeltaTime) override;

	/** Poll for controller state and send events if needed */
	virtual void SendControllerEvents() override;

	/** Set which MessageHandler will get the events from SendControllerEvents. */
	virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;

	/** Exec handler to allow console commands to be passed through for debugging */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/** IForceFeedbackSystem pass through functions **/
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override;

	/** SteamVR Sytem Handler **/
	IVRSystem* SteamVRSystem = nullptr;

	/** Current SteamVR Error **/
	EVRInputError SteamVRError = VRInputError_None;

	struct FControllerState
	{
		/** Which hand this controller is representing */
		EControllerHand Hand;

		/** If packet num matches that on your prior call, then the controller state hasn't been changed since
		  * your last call and there is no need to process it. */
		uint32 PacketNum;

		/** Touchpad analog values */
		float TouchPadXAnalog;
		float TouchPadYAnalog;

		/** trigger analog value */
		float TriggerAnalog;

		/** Knuckles Controller Axes */
		float HandGripAnalog;
		float IndexGripAnalog;
		float MiddleGripAnalog;
		float RingGripAnalog;
		float PinkyGripAnalog;
		float ThumbGripAnalog;

		float ThumbIndexSplayAnalog;
		float IndexMiddleSplayAnalog;
		float MiddleRingSplayAnalog;
		float RingPinkySplayAnalog;

		/** Last frame's button states, so we only send events on edges */
		bool ButtonStates[ESteamVRInputButton::TotalButtonCount];

		/** Next time a repeat event should be generated for each button */
		double NextRepeatTime[ESteamVRInputButton::TotalButtonCount];

		/** Value for force feedback on this controller hand */
		float ForceFeedbackValue;
	};

	/** Mappings between tracked devices and 0 indexed controllers */
	int32 NumControllersMapped;
	int32 NumTrackersMapped;
	int32 DeviceToControllerMap[k_unMaxTrackedDeviceCount];
	int32 UnrealControllerIdAndHandToDeviceIdMap[SteamVRInputDeviceConstants::MaxUnrealControllers][k_unMaxTrackedDeviceCount];
	int32 UnrealControllerHandUsageCount[CONTROLLERS_PER_PLAYER];

	/** Controller states */
	FControllerState ControllerStates[SteamVRInputDeviceConstants::MaxControllers];

	TArray<FSteamVRAction> Actions;
	VRActionSetHandle_t MainActionSet;

	VRActionHandle_t VRSkeletalHandleLeft;
	VRActionHandle_t VRSkeletalHandleRight;

	VRSkeletalSummaryData_t VRSkeletalSummaryDataLeft;
	VRSkeletalSummaryData_t VRSkeletalSummaryDataRight;

	EVRInputError LastInputError = vr::VRInputError_None;

	/** Delay before sending a repeat message after a button was first pressed */
	float InitialButtonRepeatDelay;

	/** Delay before sending a repeat message after a button has been pressed for a while */
	float ButtonRepeatDelay;

	/** Mapping of controller buttons */
	FGamepadKeyNames::Type Buttons[vr::k_unMaxTrackedDeviceCount][ESteamVRInputButton::TotalButtonCount];

private:
	/** VR Input Error Handler **/
	void GetInputError(EVRInputError InputError, FString InputAction);

	/* Message handler */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	/** Previous SteamVR Error **/
	EVRInputError PrevSteamVRError = VRInputError_None;

	// Motion Controller Helper Functions
	void SetUnrealControllerIdToControllerIndex(const int32 UnrealControllerId, const EControllerHand Hand, int32 value);
	void InitControllerMappings();
	void InitKnucklesControllerKeys();

	// Action event processing
	void SendActionInputEvents();
	void SendSkeletalInputEvents();

	// Controller Bindings Helper Functions (Editor Only)
#if WITH_EDITOR
	FDelegateHandle ActionMappingsChangedHandle;
	void BuildDefaultActionBindings(const FString& BindingsDir, TArray<TSharedPtr<FJsonValue>>& InOutDefaultBindings, TArray<FSteamVRAction>& InActionsArray, TArray<FInputMapping>& InInputMapping);
	void GenerateActionBindings(TArray<FInputMapping> &InInputMapping, TArray<TSharedPtr<FJsonValue>> &JsonValuesArray);
#endif

	// Action Manifest and Bindings Helper Functions
	void BuildActionManifest();

	void RegisterDeviceChanges();
	bool RegisterController(uint32 DeviceIndex);
	void DetectHandednessSwap();
	bool RegisterTracker(uint32 DeviceIndex);
	void UnregisterController(uint32 DeviceIndex);
	void UnregisterTracker(uint32 DeviceIndex);
	void UnregisterDevice(uint32 DeviceIndex);
#pragma region EpicUtilityFunctions
	// --------------------------------------------------------------------------------------------
	// NOTE: These are retained Utility Functions Epic to keep Engine SteamVR Plugin compatibility
	//       in case of a future Engine Merge.
	// TODO: If future merge modifications in the Engine's Input Editor could be a cleaner approach
	// --------------------------------------------------------------------------------------------

	// Hack to prefer emitting MotionController keys for action events
	static bool MatchKeyNamePrefix(const FKey& Key, const TCHAR* Prefix)
	{
		return Key.GetFName().ToString().StartsWith(Prefix);
	};

	static bool MatchKeyNameSuffix(const FKey& Key, const TCHAR* Suffix)
	{
		return Key.GetFName().ToString().EndsWith(Suffix);
	};

	// Finds an axis key mapping from a list of mapping with the following preferences:
	// 1. Tries to find a FloatAxis key that starts with "MotionController" and ends with "X"
	// 2. Tries to find a FloatAxis key that starts with "MotionController" and ends with "Y"
	// 3. Any to find a FloatAxis key that starts with "MotionController"
	// 4. Any FloatAxis that ends with "X"
	// 5. Any FloatAxis that ends with "Y"
	// 6. Any FloatAxis
	// 7. Any Valid key.
	// If case 1 or 3 is matched bOutIsXAxis will be set to true. 
	static FName FindAxisKeyMapping(TArray<FInputAxisKeyMapping>& Mappings, bool& bOutIsXAxis)
	{
		FInputAxisKeyMapping* Found = nullptr;
		bOutIsXAxis = false;
		// First filter out all floatAxes, as all except the 5th case require a float axis.
		TArray<FInputAxisKeyMapping> FloatMappings = Mappings.FilterByPredicate([](const FInputAxisKeyMapping& Mapping)
		{
			return Mapping.Key.IsFloatAxis();
		});

		// If there were no float axis key bindings, return the first valid mapping
		if (FloatMappings.Num() == 0)
		{
			Found = Mappings.FindByPredicate([](FInputAxisKeyMapping& Mapping)
			{
				return Mapping.Key.IsValid();
			});

			if (Found != nullptr)
			{
				return Found->Key.GetFName();
			}
			else
			{
				return FName();
			}
		}

		// Then get all mappings with keys starting with "MotionController"
		TArray<FInputAxisKeyMapping> MotionControllerMappings = FloatMappings.FilterByPredicate([](const FInputAxisKeyMapping& Mapping)
		{
			return MatchKeyNamePrefix(Mapping.Key, TEXT("MotionController"));
		});

		// If there are no MotionController keys, search through all FloatAxes:
		TArray<FInputAxisKeyMapping>& MappingsSubset = MotionControllerMappings.Num() == 0 ? FloatMappings : MotionControllerMappings;

		Found = FloatMappings.FindByPredicate([](const FInputAxisKeyMapping& Mapping)
		{
			return MatchKeyNameSuffix(Mapping.Key, TEXT("X"));
		});
		if (Found != nullptr)
		{
			bOutIsXAxis = true;
			return Found->Key.GetFName();
		}

		Found = FloatMappings.FindByPredicate([](const FInputAxisKeyMapping& Mapping)
		{
			return MatchKeyNameSuffix(Mapping.Key, TEXT("Y"));
		});
		if (Found != nullptr)
		{
			return Found->Key.GetFName();
		}


		Found = FloatMappings.FindByPredicate([](const FInputAxisKeyMapping& Mapping)
		{
			return Mapping.Key.IsValid();
		});
		if (Found != nullptr)
		{
			return Found->Key.GetFName();
		}
		else
		{
			return FName();
		}
	}

	/** Returns the concatenation of two strings, skipping all characters at the beginning of string B that match the beginning of string A and
		all characters at the end of string A that match the end of string B.
		Example: passing in "MoveUpAction" and "MoveRightAction" should result in "MoveUpRightAction"
		If the strings have no common suffix or prefix, the result will simply be the concatenation of both strings.
		If the strings are identical, returns the first string.

		The algorithm treats separator characters ' ', '_' and '/' differently. If either the suffix begins with one or the prefix ends with one,
		the function will keep one of them in the resulting string.
		Example "move_up_action" and "move_right_action" will result in "move_up_right_action" and not "move_upright_action"
	*/
	static FString MergeActionNames(const FString& A, const FString& B)
	{
		if (A.Equals(B, ESearchCase::CaseSensitive))
		{
			return A;
		}
		const int LastA = A.Len() - 1;
		const int LastB = B.Len() - 1;
		const int MinLen = (LastA < LastB) ? A.Len() : B.Len();

		int CommonPrefix = 0;
		int CommonSuffix = 0;
		for (; CommonPrefix < MinLen && A[CommonPrefix] == B[CommonPrefix]; CommonPrefix++)
		{
			/* intentionally blank */
		}

		for (; CommonSuffix < MinLen && A[LastA - CommonSuffix] == B[LastB - CommonSuffix]; CommonSuffix++)
		{
			/* intentionally blank */
		}

		// If either the common prefix ends with or the common suffix begins with a space, an underscore or a dash, keep one of them.
		if (CommonPrefix > 0 && (A[CommonPrefix - 1] == TEXT(' ') || A[CommonPrefix - 1] == TEXT('_') || A[CommonPrefix - 1] == TEXT('-')))
		{
			CommonPrefix--;
		}
		else if (CommonSuffix > 0 && (A[LastA - CommonSuffix + 1] == TEXT(' ') || A[LastA - CommonSuffix + 1] == TEXT('_') || A[LastA - CommonSuffix + 1] == TEXT('-')))
		{
			CommonSuffix--;
		}

		return A.LeftChop(CommonSuffix) + B.RightChop(CommonPrefix);
	}
};

#pragma endregion Kept for Backwards Compatibility
