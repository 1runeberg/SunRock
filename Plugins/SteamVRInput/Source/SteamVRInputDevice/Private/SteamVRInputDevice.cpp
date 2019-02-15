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

#include "SteamVRInputDevice.h"
#include "IInputInterface.h"
#include "HAL/FileManagerGeneric.h"
#include "Misc/FileHelper.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "SteamVRInputDevice"
DEFINE_LOG_CATEGORY_STATIC(LogSteamVRInputDevice, Log, All);

FSteamVRInputDevice::FSteamVRInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) 
	: MessageHandler(InMessageHandler)
{
	FMemory::Memzero(ControllerStates, sizeof(ControllerStates));
	NumControllersMapped = 0;
	NumTrackersMapped = 0;

	InitialButtonRepeatDelay = 0.2f;
	ButtonRepeatDelay = 0.1f;

	InitControllerMappings();
	InitKnucklesControllerKeys();
	BuildActionManifest();

	// Initialize OpenVR
	EVRInitError SteamVRInitError = VRInitError_None;
	SteamVRSystem = VR_Init(&SteamVRInitError, VRApplication_Scene);

	if (SteamVRInitError != VRInitError_None)
	{
		SteamVRSystem = NULL;
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("Unable to init SteamVR runtime %u.%u.%u: %s"), k_nSteamVRVersionMajor, k_nSteamVRVersionMinor, k_nSteamVRVersionBuild, *FString(VR_GetVRInitErrorAsEnglishDescription(SteamVRInitError)));
		return;
	}
	else
	{
		UE_LOG(LogSteamVRInputDevice, Display, TEXT("SteamVR runtime %u.%u.%u initialised with status: %s"), k_nSteamVRVersionMajor, k_nSteamVRVersionMinor, k_nSteamVRVersionBuild, *FString(VR_GetVRInitErrorAsEnglishDescription(SteamVRInitError)));

		for (unsigned int id = 0; id < k_unMaxTrackedDeviceCount; ++id)
		{
			ETrackedDeviceClass trackedDeviceClass = SteamVRSystem->GetTrackedDeviceClass(id);
			char buf[32];
			
			if (trackedDeviceClass != ETrackedDeviceClass::TrackedDeviceClass_Invalid)
			{
				uint32 StringBytes = SteamVRSystem->GetStringTrackedDeviceProperty(id, ETrackedDeviceProperty::Prop_ModelNumber_String, buf, sizeof(buf));
				FString stringCache = *FString(UTF8_TO_TCHAR(buf));
				UE_LOG(LogSteamVRInputDevice, Display, TEXT("Found the following device: [%i] %s"), id, *stringCache);
			}
		}
	}
}

FSteamVRInputDevice::~FSteamVRInputDevice()
{
#if WITH_EDITOR
	if (ActionMappingsChangedHandle.IsValid())
	{
		FEditorDelegates::OnActionAxisMappingsChanged.Remove(ActionMappingsChangedHandle);
		ActionMappingsChangedHandle.Reset();
	}
#endif
}

void FSteamVRInputDevice::Tick(float DeltaTime)
{
	// Check for changes in active controller
	if (SteamVRSystem != NULL)
	{
		RegisterDeviceChanges();
		DetectHandednessSwap();
	}

	// Send Skeletal data
	// TODO: Add check to control skeletal data flow
	SendSkeletalInputEvents();
}

void FSteamVRInputDevice::SendSkeletalInputEvents()
{
	if (SteamVRSystem != NULL && VRInput() != nullptr)
	{
		VRActiveActionSet_t ActiveActionSets[] = {
			{
				MainActionSet,
				k_ulInvalidInputValueHandle,
				k_ulInvalidActionSetHandle
			}
		};

		EVRInputError Err = VRInput()->UpdateActionState(ActiveActionSets, sizeof(VRActiveActionSet_t), 1);
		if (Err != VRInputError_None && Err != LastInputError)
		{
			UE_LOG(LogSteamVRInputDevice, Warning, TEXT("UpdateActionState returned error: %d"), (int32)Err);
			return;
		}
		Err = LastInputError;

		// Process Skeletal Data
		for (uint32 DeviceIndex = 0; DeviceIndex < k_unMaxTrackedDeviceCount; ++DeviceIndex)
		{
			// see what kind of hardware this is
			ETrackedDeviceClass DeviceClass = SteamVRSystem->GetTrackedDeviceClass(DeviceIndex);

			// skip non-controller or non-tracker devices
			if (DeviceClass != TrackedDeviceClass_Controller)
			{
				continue;
			}

			FControllerState& ControllerState = ControllerStates[DeviceIndex];
			EVRSkeletalTrackingLevel vrSkeletalTrackingLevel;
			bool bIsKnucklesLeftPresent = false;
			bool bIsKnucklesRightPresent = false;

			// Set Skeletal Action Handles
			Err = VRInput()->GetActionHandle(TCHAR_TO_ANSI(*FString(TEXT("/actions/main/in/SkeletonLeft"))), &VRSkeletalHandleLeft);
			if ((Err != VRInputError_None || !VRSkeletalHandleLeft) && Err != LastInputError)
			{
				UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Couldn't get skeletal action handle for Left Skeleton. Error: %d"), (int32)Err);
			}
			else
			{
				// Check for Left Knuckles
				VRInput()->GetSkeletalTrackingLevel(VRSkeletalHandleLeft, &vrSkeletalTrackingLevel);
				//UE_LOG(LogKnucklesLivelinkSource, Warning, TEXT("[KNUCKLES VR CONTROLLER] Left Skeletal Tracking Level: %i"), vrSkeletalTrackingLevel);

				if (vrSkeletalTrackingLevel >= VRSkeletalTracking_Partial)
				{
					bIsKnucklesLeftPresent = true;
					//UE_LOG(LogKnucklesLivelinkSource, Warning, TEXT("[KNUCKLES LIVELINK] Knuckles Left found and is ACTIVE"));
				}
			}
			Err = LastInputError;

			Err = VRInput()->GetActionHandle(TCHAR_TO_ANSI(*FString(TEXT("/actions/main/in/SkeletonRight"))), &VRSkeletalHandleRight);
			if ((Err != VRInputError_None || !VRSkeletalHandleRight) && Err != LastInputError)
			{
				UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Couldn't get skeletal action handle for Right Skeleton. Error: %d"), (int32)Err);
			}
			else
			{
				// Check for Right Knuckles
				VRInput()->GetSkeletalTrackingLevel(VRSkeletalHandleRight, &vrSkeletalTrackingLevel);
				//UE_LOG(LogKnucklesLivelinkSource, Warning, TEXT("[KNUCKLES VR CONTROLLER] Left Skeletal Tracking Level: %i"), vrSkeletalTrackingLevel);

				if (vrSkeletalTrackingLevel >= VRSkeletalTracking_Partial)
				{
					bIsKnucklesRightPresent = true;
					//UE_LOG(LogKnucklesLivelinkSource, Warning, TEXT("[KNUCKLES LIVELINK] Knuckles Left found and is ACTIVE"));
				}
			}
			Err = LastInputError;

			// Get Skeletal Summary Data
			VRActionHandle_t ActiveSkeletalHand;
			VRSkeletalSummaryData_t ActiveSkeletalSummaryData;

			if (bIsKnucklesLeftPresent && SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_LeftHand)
			{
				ActiveSkeletalHand = VRSkeletalHandleLeft;
				ActiveSkeletalSummaryData = VRSkeletalSummaryDataLeft;
			}
			else if (bIsKnucklesRightPresent && SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_RightHand)
			{
				ActiveSkeletalHand = VRSkeletalHandleRight;
				ActiveSkeletalSummaryData = VRSkeletalSummaryDataRight;
			}
			else
			{
				continue;
			}

			// Get Skeletal Summary Data
			Err = VRInput()->GetSkeletalSummaryData(ActiveSkeletalHand, &ActiveSkeletalSummaryData);
			if (Err != VRInputError_None && Err != LastInputError)
			{
				UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Unable to read Skeletal Summary Data: %d"), (int32)Err);
			}
			LastInputError = Err;

			// Knuckles Finger Curls
			if (ControllerState.IndexGripAnalog != ActiveSkeletalSummaryData.flFingerCurl[EVRFinger::VRFinger_Index])
			{
				const FGamepadKeyNames::Type AxisButton = (SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_LeftHand) ?
					KnucklesVRControllerKeyNames::SteamVR_Knuckles_Left_Finger_Index_Curl : KnucklesVRControllerKeyNames::SteamVR_Knuckles_Right_Finger_Index_Curl;
				ControllerState.IndexGripAnalog = ActiveSkeletalSummaryData.flFingerCurl[EVRFinger::VRFinger_Index];
				MessageHandler->OnControllerAnalog(AxisButton, 0, ControllerState.IndexGripAnalog);
			}

			if (ControllerState.MiddleGripAnalog != ActiveSkeletalSummaryData.flFingerCurl[EVRFinger::VRFinger_Middle])
			{
				const FGamepadKeyNames::Type AxisButton = (SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_LeftHand) ?
					KnucklesVRControllerKeyNames::SteamVR_Knuckles_Left_Finger_Middle_Curl : KnucklesVRControllerKeyNames::SteamVR_Knuckles_Right_Finger_Middle_Curl;
				ControllerState.MiddleGripAnalog = ActiveSkeletalSummaryData.flFingerCurl[EVRFinger::VRFinger_Middle];
				MessageHandler->OnControllerAnalog(AxisButton, 0, ControllerState.MiddleGripAnalog);
			}

			if (ControllerState.RingGripAnalog != ActiveSkeletalSummaryData.flFingerCurl[EVRFinger::VRFinger_Ring])
			{
				const FGamepadKeyNames::Type AxisButton = (SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_LeftHand) ?
					KnucklesVRControllerKeyNames::SteamVR_Knuckles_Left_Finger_Ring_Curl : KnucklesVRControllerKeyNames::SteamVR_Knuckles_Right_Finger_Ring_Curl;
				ControllerState.RingGripAnalog = ActiveSkeletalSummaryData.flFingerCurl[EVRFinger::VRFinger_Ring];
				MessageHandler->OnControllerAnalog(AxisButton, 0, ControllerState.RingGripAnalog);
			}

			if (ControllerState.PinkyGripAnalog != ActiveSkeletalSummaryData.flFingerCurl[EVRFinger::VRFinger_Pinky])
			{
				const FGamepadKeyNames::Type AxisButton = (SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_LeftHand) ?
					KnucklesVRControllerKeyNames::SteamVR_Knuckles_Left_Finger_Pinky_Curl : KnucklesVRControllerKeyNames::SteamVR_Knuckles_Right_Finger_Pinky_Curl;
				ControllerState.PinkyGripAnalog = ActiveSkeletalSummaryData.flFingerCurl[EVRFinger::VRFinger_Pinky];
				MessageHandler->OnControllerAnalog(AxisButton, 0, ControllerState.PinkyGripAnalog);
			}

			if (ControllerState.ThumbGripAnalog != ActiveSkeletalSummaryData.flFingerCurl[EVRFinger::VRFinger_Thumb])
			{
				const FGamepadKeyNames::Type AxisButton = (SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_LeftHand) ?
					KnucklesVRControllerKeyNames::SteamVR_Knuckles_Left_Finger_Thumb_Curl : KnucklesVRControllerKeyNames::SteamVR_Knuckles_Right_Finger_Thumb_Curl;
				ControllerState.ThumbGripAnalog = ActiveSkeletalSummaryData.flFingerCurl[EVRFinger::VRFinger_Thumb];
				MessageHandler->OnControllerAnalog(AxisButton, 0, ControllerState.ThumbGripAnalog);
			}


			// Knuckles Finger Splays
			if (ControllerState.ThumbIndexSplayAnalog != ActiveSkeletalSummaryData.flFingerSplay[EVRFingerSplay::VRFingerSplay_Thumb_Index])
			{
				const FGamepadKeyNames::Type AxisButton = (SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_LeftHand) ?
					KnucklesVRControllerKeyNames::SteamVR_Knuckles_Left_Finger_ThumbIndex_Splay : KnucklesVRControllerKeyNames::SteamVR_Knuckles_Right_Finger_ThumbIndex_Splay;
				ControllerState.ThumbIndexSplayAnalog = ActiveSkeletalSummaryData.flFingerSplay[EVRFingerSplay::VRFingerSplay_Thumb_Index];
				MessageHandler->OnControllerAnalog(AxisButton, 0, ControllerState.ThumbIndexSplayAnalog);
			}

			if (ControllerState.IndexMiddleSplayAnalog != ActiveSkeletalSummaryData.flFingerSplay[EVRFingerSplay::VRFingerSplay_Index_Middle])
			{
				const FGamepadKeyNames::Type AxisButton = (SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_LeftHand) ?
					KnucklesVRControllerKeyNames::SteamVR_Knuckles_Left_Finger_IndexMiddle_Splay : KnucklesVRControllerKeyNames::SteamVR_Knuckles_Right_Finger_IndexMiddle_Splay;
				ControllerState.IndexMiddleSplayAnalog = ActiveSkeletalSummaryData.flFingerSplay[EVRFingerSplay::VRFingerSplay_Index_Middle];
				MessageHandler->OnControllerAnalog(AxisButton, 0, ControllerState.IndexMiddleSplayAnalog);
			}

			if (ControllerState.MiddleRingSplayAnalog != ActiveSkeletalSummaryData.flFingerSplay[EVRFingerSplay::VRFingerSplay_Middle_Ring])
			{
				const FGamepadKeyNames::Type AxisButton = (SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_LeftHand) ?
					KnucklesVRControllerKeyNames::SteamVR_Knuckles_Left_Finger_RingPinky_Splay : KnucklesVRControllerKeyNames::SteamVR_Knuckles_Right_Finger_RingPinky_Splay;
				ControllerState.MiddleRingSplayAnalog = ActiveSkeletalSummaryData.flFingerSplay[EVRFingerSplay::VRFingerSplay_Middle_Ring];
				MessageHandler->OnControllerAnalog(AxisButton, 0, ControllerState.MiddleRingSplayAnalog);
			}

			if (ControllerState.RingPinkySplayAnalog != ActiveSkeletalSummaryData.flFingerSplay[EVRFingerSplay::VRFingerSplay_Ring_Pinky])
			{
				const FGamepadKeyNames::Type AxisButton = (SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex) == ETrackedControllerRole::TrackedControllerRole_LeftHand) ?
					KnucklesVRControllerKeyNames::SteamVR_Knuckles_Left_Finger_RingPinky_Splay : KnucklesVRControllerKeyNames::SteamVR_Knuckles_Right_Finger_RingPinky_Splay;
				ControllerState.RingPinkySplayAnalog = ActiveSkeletalSummaryData.flFingerSplay[EVRFingerSplay::VRFingerSplay_Ring_Pinky];
				MessageHandler->OnControllerAnalog(AxisButton, 0, ControllerState.RingPinkySplayAnalog);
			}
		}
	}
}

void FSteamVRInputDevice::SendControllerEvents()
{
	SendActionInputEvents();
}

void FSteamVRInputDevice::SendActionInputEvents()
{
	if (SteamVRSystem != NULL && VRInput() != nullptr)
	{
		VRActiveActionSet_t ActiveActionSets[] = {
			{
				MainActionSet,
				k_ulInvalidInputValueHandle,
				k_ulInvalidActionSetHandle
			}
		};

		EVRInputError Err = VRInput()->UpdateActionState(ActiveActionSets, sizeof(VRActiveActionSet_t), 1);
		if (Err != VRInputError_None)
		{
			UE_LOG(LogSteamVRInputDevice, Warning, TEXT("UpdateActionState returned error: %d"), (int32)Err);
			return;
		}

		// Go through Action Sets
		for (auto& Action : Actions)
		{
			switch (Action.Type)
			{
			case EActionType::Boolean:
			{
				InputDigitalActionData_t Data;
				Err = VRInput()->GetDigitalActionData(Action.Handle, &Data, sizeof(Data), k_ulInvalidInputValueHandle);
				if (Err == VRInputError_None)
				{
					if (Data.bState != Action.bState)
					{
						Action.bState = Data.bState;
						if (Action.bState)
						{
							MessageHandler->OnControllerButtonPressed(Action.ActionKey_X, 0, /*IsRepeat =*/false);
						}
						else
						{
							MessageHandler->OnControllerButtonReleased(Action.ActionKey_X, 0, /*IsRepeat =*/false);
						}
					}
				}
				// If the current error is the same as the last frame's error, don't log it again to avoid spamming the log
				else if (Err != Action.LastError)
				{
					UE_LOG(LogSteamVRInputDevice, Warning, TEXT("GetDigitalActionData for %s returned error: %d"), *Action.Name.ToString(), (int32)Err);

				}
				Action.LastError = Err;
			}
			break;
			case EActionType::Vector1:
			case EActionType::Vector2:
			case EActionType::Vector3:
			{
				InputAnalogActionData_t Data;
				Err = VRInput()->GetAnalogActionData(Action.Handle, &Data, sizeof(Data), k_ulInvalidInputValueHandle);
				if (Err == VRInputError_None)
				{
					if (!Action.ActionKey_X.IsNone() && Data.x != Action.Value.X)
					{
						Action.Value.X = Data.x;
						MessageHandler->OnControllerAnalog(Action.ActionKey_X, 0, Action.Value.X);
					}
					if (!Action.ActionKey_Y.IsNone() && Data.y != Action.Value.Y)
					{
						Action.Value.Y = Data.y;
						MessageHandler->OnControllerAnalog(Action.ActionKey_Y, 0, Action.Value.Y);
					}
					if (!Action.ActionKey_Z.IsNone() && Data.z != Action.Value.Z)
					{
						Action.Value.Z = Data.z;
						MessageHandler->OnControllerAnalog(Action.ActionKey_Z, 0, Action.Value.Z);
					}
				}
				// If the current error is the same as the last frame's error, don't log it again to avoid spamming the log
				else if (Err != Action.LastError)
				{
					UE_LOG(LogSteamVRInputDevice, Warning, TEXT("GetAnalogActionData for %s returned error: %d"), *Action.Name.ToString(), (int32)Err);
				}
				Action.LastError = Err;
			}
			break;
			default:
				// Other action types are currently unsupported.
				break;
			}
		}
	}
}

void FSteamVRInputDevice::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

bool FSteamVRInputDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return false;
}

void FSteamVRInputDevice::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	// Empty on purpose
}

void FSteamVRInputDevice::SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values)
{
	// Empty on purpose
}

void FSteamVRInputDevice::SetUnrealControllerIdToControllerIndex(const int32 UnrealControllerId, const EControllerHand Hand, int32 value)
{
	UnrealControllerIdAndHandToDeviceIdMap[UnrealControllerId][(int32)Hand] = value;
}

void FSteamVRInputDevice::InitControllerMappings()
{
	for (int32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
	{
		DeviceToControllerMap[i] = INDEX_NONE;
	}

	for (int32 UnrealControllerIndex = 0; UnrealControllerIndex < SteamVRInputDeviceConstants::MaxUnrealControllers; ++UnrealControllerIndex)
	{
		for (int32 HandIndex = 0; HandIndex < vr::k_unMaxTrackedDeviceCount; ++HandIndex)
		{
			SetUnrealControllerIdToControllerIndex(UnrealControllerIndex, (EControllerHand)HandIndex, INDEX_NONE);
		}
	}

	for (int32& Count : UnrealControllerHandUsageCount)
	{
		Count = 0;
	}
}

#if WITH_EDITOR
void FSteamVRInputDevice::BuildDefaultActionBindings(const FString& BindingsDir, TArray<TSharedPtr<FJsonValue>>& InOutDefaultBindings, TArray<FSteamVRAction>& InActionsArray, TArray<FInputMapping>& InInputMapping)
{
	IFileManager& FileManager = FFileManagerGeneric::Get();

	TSet<FString> ExistingBindings;
	for (const TSharedPtr<FJsonValue>& Value : InOutDefaultBindings)
	{
		const TSharedPtr<FJsonObject>* Object;
		FString ControllerType;
		if (Value.IsValid() && Value->TryGetObject(Object) && (*Object)->TryGetStringField(TEXT("controller_type"), ControllerType) && !ControllerType.IsEmpty())
		{
			ExistingBindings.Emplace(ControllerType);
		}
	}

	// Create the directory if it doesn't exist.
	if (!FileManager.DirectoryExists(*BindingsDir))
	{
		FileManager.MakeDirectory(*BindingsDir);
	}

	for (auto& Item : CommonControllerTypes)
	{

		// Skip if the controller type has already be defined
		if (ExistingBindings.Contains(Item.Key))
		{
			continue;
		}

		// Create a unique file path for the generated file.
		FString BindingsPath = FileManager.ConvertToAbsolutePathForExternalAppForRead(*FString::Printf(TEXT("%s/%s.json"), *BindingsDir, Item.Key));
		int count = 0;
		while (FileManager.FileExists(*BindingsPath) && FileManager.FileSize(*BindingsPath) > 0)
		{
			BindingsPath = FileManager.ConvertToAbsolutePathForExternalAppForRead(*FString::Printf(TEXT("%s/%s_%d.json"), *BindingsDir, Item.Key, ++count));
		}

		// Creating bindings file
		TSharedRef<FJsonObject> BindingsStub = MakeShareable(new FJsonObject());
		BindingsStub->SetStringField(TEXT("name"), *FText::Format(NSLOCTEXT("SteamVR", "DefaultBindingsFor", "Default bindings for {0}"), Item.Value).ToString());
		BindingsStub->SetStringField(TEXT("controller_type"), Item.Key);

		// Create Action Bindings in JSON Format
		TArray<TSharedPtr<FJsonValue>> JsonValuesArray;
		GenerateActionBindings(InInputMapping, JsonValuesArray);

		//Create Action Set
		TSharedRef<FJsonObject> ActionSetJsonObject = MakeShareable(new FJsonObject());
		ActionSetJsonObject->SetArrayField(TEXT("sources"), JsonValuesArray);

		// TODO: Read from mappings instead (lowpri) - perhaps more efficient pulling from template file?
		// TODO: Only generate for supported controllers
		// Add Skeleton Mappings
		TArray<TSharedPtr<FJsonValue>> SkeletonValuesArray;

		// Add Skeleton: Left Hand 
		TSharedRef<FJsonObject> SkeletonLeftJsonObject = MakeShareable(new FJsonObject());
		SkeletonLeftJsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_SKELETON_LEFT));
		SkeletonLeftJsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_USER_SKEL_LEFT));

		TSharedRef<FJsonValueObject> SkeletonLeftJsonValueObject = MakeShareable(new FJsonValueObject(SkeletonLeftJsonObject));
		SkeletonValuesArray.Add(SkeletonLeftJsonValueObject);

		// Add Skeleton: Right Hand
		TSharedRef<FJsonObject> SkeletonRightJsonObject = MakeShareable(new FJsonObject());
		SkeletonRightJsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_SKELETON_RIGHT));
		SkeletonRightJsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_USER_SKEL_RIGHT));

		TSharedRef<FJsonValueObject> SkeletonRightJsonValueObject = MakeShareable(new FJsonValueObject(SkeletonRightJsonObject));
		SkeletonValuesArray.Add(SkeletonRightJsonValueObject);

		// Add Skeleton Input Array To Action Set
		ActionSetJsonObject->SetArrayField(TEXT("skeleton"), SkeletonValuesArray);

		// TODO: Read from mappings instead (lowpri) - see similar note to Skeleton Mappings
		// Add Haptic Mappings
		TArray<TSharedPtr<FJsonValue>> HapticValuesArray;

		// Add Haptic: Left Hand 
		TSharedRef<FJsonObject> HapticLeftJsonObject = MakeShareable(new FJsonObject());
		HapticLeftJsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_VIBRATE_LEFT));
		HapticLeftJsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_USER_VIB_LEFT));

		TSharedRef<FJsonValueObject> HapticLeftJsonValueObject = MakeShareable(new FJsonValueObject(HapticLeftJsonObject));
		HapticValuesArray.Add(HapticLeftJsonValueObject);

		// Add Haptic: Right Hand
		TSharedRef<FJsonObject> HapticRightJsonObject = MakeShareable(new FJsonObject());
		HapticRightJsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_VIBRATE_RIGHT));
		HapticRightJsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_USER_VIB_RIGHT));

		TSharedRef<FJsonValueObject> HapticRightJsonValueObject = MakeShareable(new FJsonValueObject(HapticRightJsonObject));
		HapticValuesArray.Add(HapticRightJsonValueObject);

		// Add Haptic Output Array To Action Set
		ActionSetJsonObject->SetArrayField(TEXT("haptics"), HapticValuesArray);

		// Create Bindings Stub that includes all Action Sets
		TSharedRef<FJsonObject> BindingsJsonObject = MakeShareable(new FJsonObject());
		BindingsJsonObject->SetObjectField(TEXT(ACTION_SET), ActionSetJsonObject);
		BindingsStub->SetObjectField(TEXT("bindings"), BindingsJsonObject);

		// Set description of Bindings stub to Project Name
		if (GConfig)
		{
			// Retrieve Project Name and Version from DefaultGame.ini
			FString ProjectName;
			FString ProjectVersion;

			GConfig->GetString(
				TEXT("/Script/EngineSettings.GeneralProjectSettings"),
				TEXT("ProjectName"),
				ProjectName,
				GGameIni
			);

			// Add Project Name and Version to Bindings stub
			BindingsStub->SetStringField(TEXT("description"), (TEXT("%s"), *ProjectName));
		}
		else
		{
			BindingsStub->SetStringField(TEXT("description"), TEXT("SteamVRInput UE4 Plugin Generated Bindings"));
		}

		// Print the stub bindings to a JSON string and save it to a file
		FString OutputJsonString;
		TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&OutputJsonString);
		FJsonSerializer::Serialize(BindingsStub, JsonWriter);
		FFileHelper::SaveStringToFile(OutputJsonString, *BindingsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

		// Add the path of the generated file the action manifest
		TSharedRef<FJsonObject> BindingObject = MakeShareable(new FJsonObject());
		BindingObject->SetStringField(TEXT("controller_type"), Item.Key);
		BindingObject->SetStringField(TEXT("binding_url"), *BindingsPath);
		InOutDefaultBindings.Add(MakeShareable(new FJsonValueObject(BindingObject)));
	}
}

void FSteamVRInputDevice::GenerateActionBindings(TArray<FInputMapping> &InInputMapping, TArray<TSharedPtr<FJsonValue>> &JsonValuesArray)
{
	for (int i = 0; i < InInputMapping.Num(); i++)
	{
		// Create Action Input
		TSharedRef<FJsonObject> ActionInputJsonObject = MakeShareable(new FJsonObject());
		//ActionInputJsonObject->SetObjectField(TEXT("force"), ActionPathJsonObject);

		// TODO: Catch curls and splays in action events
		if (InInputMapping[i].InputKey.ToString().Contains(TEXT("Curl"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
			InInputMapping[i].InputKey.ToString().Contains(TEXT("Splay"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			continue;

		// TODO: Bitmask - consider button press abstractions; perhaps several enums?
		// Set Input Type
		bool bIsAxis;
		bool bIsTrigger;
		bool bIsThumbstick;
		bool bIsTrackpad;
		bool bIsGrip;
		bool bIsCapSense;
		bool bIsLeft;
		bool bIsFaceButton1;   // TODO: Better way of abstracting buttons
		bool bIsFaceButton2;

		// Set Cache Vars
		FName CacheMode;
		FString CacheType;
		FString CachePath;

		if (InInputMapping[i].InputKey.ToString().Contains(TEXT("_X"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
			InInputMapping[i].InputKey.ToString().Contains(TEXT("_Y"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
			InInputMapping[i].InputKey.ToString().Contains(TEXT("Grip"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
			InInputMapping[i].InputKey.ToString().Contains(TEXT("Axis"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			bIsAxis = true;
		}
		else
		{
			bIsAxis = false;
		}

		bIsTrigger = InInputMapping[i].InputKey.ToString().Contains(TEXT("Trigger"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		bIsThumbstick = InInputMapping[i].InputKey.ToString().Contains(TEXT("Thumbstick"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		bIsTrackpad = InInputMapping[i].InputKey.ToString().Contains(TEXT("Trackpad"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		bIsGrip = InInputMapping[i].InputKey.ToString().Contains(TEXT("Grip"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		bIsCapSense = InInputMapping[i].InputKey.ToString().Contains(TEXT("CapSense"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		bIsLeft = InInputMapping[i].InputKey.ToString().Contains(TEXT("Left"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		bIsFaceButton1 = InInputMapping[i].InputKey.ToString().Contains(TEXT("FaceButton1"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		bIsFaceButton2 = InInputMapping[i].InputKey.ToString().Contains(TEXT("FaceButton2"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		// Set Cache Mode
		CacheMode = bIsTrigger || bIsGrip ? FName(TEXT("trigger")) : FName(TEXT("button"));
		CacheMode = bIsTrackpad ? FName(TEXT("trackpad")) : CacheMode;
		CacheMode = bIsGrip ? FName(TEXT("force_sensor")) : CacheMode;
		CacheMode = bIsThumbstick ? FName(TEXT("joystick")) : CacheMode;

		// Set Cache Path
		if (bIsTrigger)
		{
			CachePath = bIsLeft ? FString(TEXT(ACTION_PATH_TRIGGER_LEFT)) : FString(TEXT(ACTION_PATH_TRIGGER_RIGHT));
		}
		else if (bIsThumbstick)
		{
			CachePath = bIsLeft ? FString(TEXT(ACTION_PATH_THUMBSTICK_LEFT)) : FString(TEXT(ACTION_PATH_THUMBSTICK_RIGHT));
		}
		else if (bIsTrackpad)
		{
			CachePath = bIsLeft ? FString(TEXT(ACTION_PATH_TRACKPAD_LEFT)) : FString(TEXT(ACTION_PATH_TRACKPAD_RIGHT));
		}
		else if (bIsGrip)
		{
			CachePath = bIsLeft ? FString(TEXT(ACTION_PATH_GRIP_LEFT)) : FString(TEXT(ACTION_PATH_GRIP_RIGHT));
		}
		else if (bIsFaceButton1)
		{
			CachePath = bIsLeft ? FString(TEXT(ACTION_PATH_BTN_A_LEFT)) : FString(TEXT(ACTION_PATH_BTN_A_RIGHT));
		}
		else if (bIsFaceButton2)
		{
			CachePath = bIsLeft ? FString(TEXT(ACTION_PATH_BTN_B_LEFT)) : FString(TEXT(ACTION_PATH_BTN_B_RIGHT));
		}

		// Create Action Source
		FActionSource ActionSource = FActionSource(CacheMode, CachePath);
		TSharedRef<FJsonObject> ActionSourceJsonObject = MakeShareable(new FJsonObject());
		ActionSourceJsonObject->SetStringField(TEXT("mode"), ActionSource.Mode.ToString());

		// Set Action Path
		ActionSourceJsonObject->SetStringField(TEXT("path"), ActionSource.Path);

		// Set Key Mappings
		for (FString Action : InInputMapping[i].Actions)
		{
			// Create Action Path
			FActionPath ActionPath = FActionPath(Action);
			TSharedRef<FJsonObject> ActionPathJsonObject = MakeShareable(new FJsonObject());
			ActionPathJsonObject->SetStringField(TEXT("output"), ActionPath.Path);

			// Set Cache Type
			if (bIsAxis)
			{
				CacheType = (bIsThumbstick || bIsTrackpad) ? FString(TEXT("position")) : FString(TEXT("pull"));
				CacheType = (bIsGrip) ? FString(TEXT("force")) : CacheType;
			}
			else
			{
				CacheType = (bIsCapSense) ? FString(TEXT("touch")) : FString(TEXT("click"));
			}

			// Set Action Input Type
			ActionInputJsonObject->SetObjectField(CacheType, ActionPathJsonObject);
		}

		// Set Inputs
		ActionSourceJsonObject->SetObjectField(TEXT("inputs"), ActionInputJsonObject);

		// Add to Sources Array
		TSharedRef<FJsonValueObject> JsonValueObject = MakeShareable(new FJsonValueObject(ActionSourceJsonObject));
		JsonValuesArray.Add(JsonValueObject);
	}
}
#endif

void FSteamVRInputDevice::BuildActionManifest()
{
	if (VRInput() != nullptr)
	{
		// Input Key Mappings - UE uses Action to Multiple Inputs, this needs to be reorganized to match 
		// Valve's which is Input to multiple actions
		TArray<FInputMapping> InputMappings;
		TArray<FName> UniqueInputs;

		// Get Project Action Settings
		Actions.Empty();
		auto InputSettings = GetDefault<UInputSettings>();
		if (InputSettings != nullptr)
		{
			// Get all Action Key Mappings
			TArray<FName> ActionNames;
			InputSettings->GetActionNames(ActionNames);
			for (const auto& ActionName : ActionNames)
			{
				TArray<FInputActionKeyMapping> Mappings;
				InputSettings->GetActionMappingByName(ActionName, Mappings);
				for (FInputActionKeyMapping Mapping : Mappings)
				{
					UniqueInputs.AddUnique(Mapping.Key.GetFName());
				}

				FInputActionKeyMapping* KeyMapping = Mappings.FindByPredicate([](FInputActionKeyMapping& Mapping)
				{
					return MatchKeyNamePrefix(Mapping.Key, TEXT("MotionController"));
				});

				if (KeyMapping == nullptr)
				{
					KeyMapping = Mappings.FindByPredicate([](FInputActionKeyMapping& Mapping) { return Mapping.Key.IsValid(); });
				}

				if (KeyMapping != nullptr)
				{
					FString ActionPath = FString(TEXT(ACTION_PATH_IN)) / ActionName.ToString();
					Actions.Add(FSteamVRAction(ActionPath, ActionName, KeyMapping->Key.GetFName(), false));
				}

			}

			// Get All Action Axis Mappings
			TArray<FName> AxisNames;
			InputSettings->GetAxisNames(AxisNames);
			for (const auto& AxisName : AxisNames)
			{
				bool bIsXAxis = false;
				TArray<FInputAxisKeyMapping> AxisMappings;
				InputSettings->GetAxisMappingByName(AxisName, AxisMappings);
				FName KeyName = FindAxisKeyMapping(AxisMappings, bIsXAxis);

				if (!KeyName.IsNone())
				{
					FString ActionPath = FString(TEXT(ACTION_PATH_IN)) / AxisName.ToString() + TEXT("_axis");
					Actions.Add(FSteamVRAction(ActionPath, AxisName, KeyName, 0.0f));

					TArray<FInputAxisKeyMapping> XMappings;
					InputSettings->GetAxisMappingByName(AxisName, XMappings);
					for (FInputAxisKeyMapping XMapping : XMappings)
					{
						UniqueInputs.AddUnique(XMapping.Key.GetFName());
					}

					// If the current axis is bound to an X axis, find the corresponding Y axis binding and create
					// a combined vector2 action from them (and if there were Z axes, create vector3 actions.)
					if (bIsXAxis)
					{
						FName YKeyName = FName(*(KeyName.ToString().LeftChop(1) + TEXT('Y')));
						FName ZKeyName = FName(*(KeyName.ToString().LeftChop(1) + TEXT('Z')));
						FName YAxisName, ZAxisName;

						for (const auto& InnerAxisName : AxisNames)
						{
							TArray<FInputAxisKeyMapping> InnerMappings;
							InputSettings->GetAxisMappingByName(InnerAxisName, InnerMappings);

							if (YAxisName.IsNone() &&
								InnerMappings.ContainsByPredicate([&YKeyName](auto& Mapping)
							{
								return Mapping.Key.GetFName() == YKeyName;
							})
								)
							{
								YAxisName = InnerAxisName;
							}

							if (ZAxisName.IsNone() &&
								InnerMappings.ContainsByPredicate([&ZKeyName](auto& Mapping)
							{
								return Mapping.Key.GetFName() == ZKeyName;
							})
								)
							{
								ZAxisName = InnerAxisName;
							}

							if (!YAxisName.IsNone() && !ZAxisName.IsNone())
							{
								break;
							}

						}

						if (!YAxisName.IsNone())
						{
							FString CombinedAxisName = MergeActionNames(AxisName.ToString(), YAxisName.ToString());
							if (!ZAxisName.IsNone())
							{
								CombinedAxisName = MergeActionNames(CombinedAxisName, ZAxisName.ToString());
								FString CombinedActionPath = FString(TEXT(ACTION_PATH_IN)) / CombinedAxisName + TEXT("_axis3d");
								Actions.Add(FSteamVRAction(CombinedActionPath, FName(*CombinedAxisName), KeyName, YKeyName, ZKeyName, FVector::ZeroVector));
							}
							else
							{
								FString CombinedActionPath = FString(TEXT(ACTION_PATH_IN)) / CombinedAxisName + TEXT("_axis2d");
								Actions.Add(FSteamVRAction(CombinedActionPath, FName(*CombinedAxisName), KeyName, YKeyName, FVector2D::ZeroVector));

								TArray<FInputAxisKeyMapping> V2Mappings;
								InputSettings->GetAxisMappingByName(AxisName, V2Mappings);
								for (FInputAxisKeyMapping V2Mapping : V2Mappings)
								{
									UniqueInputs.AddUnique(V2Mapping.Key.GetFName());
								}
							}
						}
					}
				}
			}

			// Reorganize to Valve style Input to Actions association
			for (FName UniqueInput : UniqueInputs)
			{
				// Create New Input Mapping from Unique Input Key
				FInputMapping NewInputMapping = FInputMapping();
				NewInputMapping.InputKey = UniqueInput;

				for (const FSteamVRAction& Action : Actions)
				{
					// Set Key Actions Linked To This Input Key
					TArray<FInputActionKeyMapping> KeyMappings;
					InputSettings->GetActionMappingByName(Action.Name, KeyMappings);
					for (FInputActionKeyMapping KeyMapping : KeyMappings)
					{
						if (UniqueInput.IsEqual(KeyMapping.Key.GetFName()))
						{
							NewInputMapping.Actions.AddUnique(Action.Path);
						}
					}

					// TODO: Cater for Vector3 (unnecessary?)
					// Check if this is a Vector2
					FName AxisNameX, AxisNameY;
					if (Action.Path.Contains(TEXT("axis2d")))
					{
						AxisNameX = FName(*Action.Name.ToString().LeftChop(1));
						AxisNameY = FName(*Action.Name.ToString().LeftChop(1));
						//UE_LOG(LogKnucklesVRController, Warning, TEXT("[KNUCKLES CONTROLLER] AxisNameX: %s"), *Action.Name.ToString().LeftChop(1));
						//UE_LOG(LogKnucklesVRController, Warning, TEXT("[KNUCKLES CONTROLLER] AxisNameY: %s"), *Action.Name.ToString().LeftChop(1));
					}
					else
					{
						AxisNameX = Action.Name;
						AxisNameY = NAME_None;
					}

					// Set Axis Actions Linked To This Input Key
					TArray<FInputAxisKeyMapping> AxisMappings;
					InputSettings->GetAxisMappingByName(AxisNameX, AxisMappings);
					for (FInputAxisKeyMapping AxisMapping : AxisMappings)
					{
						if (UniqueInput.IsEqual(AxisMapping.Key.GetFName()))
						{
							NewInputMapping.Actions.AddUnique(Action.Path);
						}
					}

					if (AxisNameY != NAME_None)
					{
						AxisMappings.Empty();
						InputSettings->GetAxisMappingByName(AxisNameY, AxisMappings);
						for (FInputAxisKeyMapping AxisMapping : AxisMappings)
						{
							if (UniqueInput.IsEqual(AxisMapping.Key.GetFName()))
							{
								NewInputMapping.Actions.AddUnique(Action.Path);
							}
						}
					}
				}

				InputMappings.Add(NewInputMapping);
			}

			// Skeletal Data
			{
				FString ConstActionPath = FString(TEXT(ACTION_PATH_SKELETON_LEFT));
				Actions.Add(FSteamVRAction(ConstActionPath, EActionType::Skeleton, true,
					FName(TEXT("Skeleton (Left)")), FString(TEXT(ACTION_PATH_SKEL_HAND_LEFT))));
			}
			{
				FString ConstActionPath = FString(TEXT(ACTION_PATH_SKELETON_RIGHT));
				Actions.Add(FSteamVRAction(ConstActionPath, EActionType::Skeleton, true,
					FName(TEXT("Skeleton (Right)")), FString(TEXT(ACTION_PATH_SKEL_HAND_RIGHT))));
			}

			// Open console
			{
				const FKey* ConsoleKey = InputSettings->ConsoleKeys.FindByPredicate([](FKey& Key) { return Key.IsValid(); });
				if (ConsoleKey != nullptr)
				{
					Actions.Add(FSteamVRAction(FString(TEXT(ACTION_PATH_OPEN_CONSOLE)), FName(TEXT("Open Console")), ConsoleKey->GetFName(), false));
				}
			}

			// Haptics
			{
				FString ConstActionPath = FString(TEXT(ACTION_PATH_VIBRATE_LEFT));
				Actions.Add(FSteamVRAction(ConstActionPath, EActionType::Vibration, true, FName(TEXT("Haptic (Left)"))));
			}
			{
				FString ConstActionPath = FString(TEXT(ACTION_PATH_VIBRATE_RIGHT));
				Actions.Add(FSteamVRAction(ConstActionPath, EActionType::Vibration, true, FName(TEXT("Haptic (Right)"))));
			}
		}

		if (Actions.Num() > 0)
		{
			// The steamvr_actions.json file is generated from internal data, so it goes to GeneratedConfig directory.
			const FString ManifestPath = FPaths::GeneratedConfigDir() / ACTION_MANIFEST;
			UE_LOG(LogSteamVRInputDevice, Display, TEXT("Manifest Path: %s"), *ManifestPath);

			// The default bindings files need to be generated by the developer, so they are stored in the project config directory.
			const FString BindingsDir = FPaths::ProjectConfigDir() / TEXT("SteamVRBindings");
			UE_LOG(LogSteamVRInputDevice, Display, TEXT("Bindings Path: %s"), *BindingsDir);

			TSharedPtr<FJsonObject> DescriptionsObject = MakeShareable(new FJsonObject);

			TArray<TSharedPtr<FJsonValue>> ActionsArray;
			for (auto Action : Actions)
			{
				TSharedRef<FJsonObject> ActionObject = MakeShareable(new FJsonObject());
				ActionObject->SetStringField(TEXT("name"), Action.Path);
				ActionObject->SetStringField(TEXT("type"), Action.TypeAsString());

				// Add hand if skeleton
				if (!Action.Skel.IsEmpty())
				{
					ActionObject->SetStringField(TEXT("skeleton"), Action.Skel);
				}

				// Add requirement field for optionals
				if (!Action.Requirement)
				{
					ActionObject->SetStringField(TEXT("requirement"), TEXT("optional"));
				}

				ActionsArray.Add(MakeShareable(new FJsonValueObject(ActionObject)));
				DescriptionsObject->SetStringField(Action.Path, Action.Name.ToString());
			}

			TArray<TSharedPtr<FJsonValue>> DefaultBindings;
			{
				IFileManager& FileManager = FFileManagerGeneric::Get();

				// Find any default bindings stored in the project bindings dir.
				// They must be saved as <PROJECT_CONFIG_DIR>/SteamVRBindings/<CONTROLLER_TYPE>.json in order to be included in the manifest.
				TArray<FString> FoundFiles;
				FileManager.FindFiles(FoundFiles, *BindingsDir, TEXT("*.json"));
				UE_LOG(LogSteamVRInputDevice, Log, TEXT("Searching for bindings files in %s"), *BindingsDir);
				for (FString& File : FoundFiles)
				{
					FString ControllerType;
					FString JsonStr;
					FString FilePath = BindingsDir / File;
					FFileHelper::LoadFileToString(JsonStr, *FilePath);
					TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonStr);
					TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
					if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
					{
						UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Invalid controller binding file %s: Invalid JSON."), *FilePath);
						continue;
					}

					if (!JsonObject->TryGetStringField(TEXT("controller_type"), ControllerType) || ControllerType.IsEmpty())
					{
						UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Invalid controller binding file %s: Missing or empty controller_type field."), *FilePath);
						continue;
					}

					TSharedRef<FJsonObject> BindingObject = MakeShareable(new FJsonObject());
					BindingObject->SetStringField(TEXT("controller_type"), *ControllerType);
					BindingObject->SetStringField(TEXT("binding_url"), *FileManager.ConvertToAbsolutePathForExternalAppForRead(*FilePath));
					DefaultBindings.Add(MakeShareable(new FJsonValueObject(BindingObject)));
				}

#if WITH_EDITOR
				BuildDefaultActionBindings(BindingsDir, DefaultBindings, Actions, InputMappings);
				check(DefaultBindings.Num());
#else
				if (DefaultBindings.Num() == 0)
				{
					UE_LOG(LogSteamVRInputDevice, Error, TEXT("No default Steam VR Input bindings found in %s."), *BindingsDir);
				}
#endif
			}

			TArray<TSharedPtr<FJsonValue>> ActionSets;
			{
				TSharedRef<FJsonObject> ActionSetObject = MakeShareable(new FJsonObject());
				ActionSetObject->SetStringField(TEXT("name"), TEXT(ACTION_SET));
				ActionSetObject->SetStringField(TEXT("usage"), TEXT("leftright"));
				ActionSets.Add(MakeShareable(new FJsonValueObject(ActionSetObject)));

				DescriptionsObject->SetStringField(TEXT(ACTION_SET), TEXT("Main Game Actions"));
			}

			DescriptionsObject->SetStringField(TEXT("language_tag"), TEXT("en"));
			TArray<TSharedPtr<FJsonValue>> Localization;
			{
				Localization.Add(MakeShareable(new FJsonValueObject(DescriptionsObject)));
			}

			TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject());
			RootObject->SetArrayField(TEXT("default_bindings"), DefaultBindings);
			RootObject->SetArrayField(TEXT("actions"), ActionsArray);
			RootObject->SetArrayField(TEXT("action_sets"), ActionSets);
			RootObject->SetArrayField(TEXT("localization"), Localization);

			// Print the JSON data to a string
			FString OutputJsonString;
			TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&OutputJsonString);
			FJsonSerializer::Serialize(RootObject, JsonWriter);

			// Save the JSON string (force UTF-8 for JSON files.)
			if (!FFileHelper::SaveStringToFile(OutputJsonString, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				UE_LOG(LogSteamVRInputDevice, Error, TEXT("Failed to save action manifest '%s'."), *ManifestPath);
				return;
			}

			EVRInputError Err = VRInput()->SetActionManifestPath(TCHAR_TO_ANSI(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ManifestPath)));

			if (Err != vr::VRInputError_None)
			{
				UE_LOG(LogSteamVRInputDevice, Error, TEXT("Failed to pass action manifest, %s, to SteamVR. Error: %d"), *ManifestPath, (int32)Err);
			}

			// Get the action set handle for our main action set
			Err = VRInput()->GetActionSetHandle(ACTION_SET, &MainActionSet);
			if (Err != vr::VRInputError_None)
			{
				UE_LOG(LogSteamVRInputDevice, Error, TEXT("Couldn't get main action set handle. Error: %d"), (int32)Err);
			}

			// Fill in Action handles for each registered action
			for (auto& Action : Actions)
			{
				vr::VRActionHandle_t Handle;
				Err = VRInput()->GetActionHandle(TCHAR_TO_ANSI(*Action.Path), &Handle);
				Action.Handle = Handle;
				if (Err != vr::VRInputError_None || !Action.Handle)
				{
					UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Couldn't get main action handle for %s. Error: %d"), *Action.Path, (int32)Err);
				}
			}

#if WITH_EDITOR
			if (!ActionMappingsChangedHandle.IsValid())
			{
				ActionMappingsChangedHandle = FEditorDelegates::OnActionAxisMappingsChanged.AddLambda([this]()
				{
					UE_LOG(LogSteamVRInputDevice, Warning, TEXT("You will need to quit and restart both SteamVR and the Editor in order to use the modified input actions or axes."));
				});
			}
#endif
		}
	}
}

#pragma region EpicControllerUtilityFunctions-SigChanges
void FSteamVRInputDevice::RegisterDeviceChanges()
{
	for (uint32 DeviceIndex = 0; DeviceIndex < k_unMaxTrackedDeviceCount; ++DeviceIndex)
	{
		// see what kind of hardware this is
		ETrackedDeviceClass DeviceClass = SteamVRSystem->GetTrackedDeviceClass(DeviceIndex);

		switch (DeviceClass)
		{
		case TrackedDeviceClass_Controller:
		{
			// Check connection status
			if (SteamVRSystem->IsTrackedDeviceConnected(DeviceIndex))
			{
				// has the controller not been mapped yet
				if (DeviceToControllerMap[DeviceIndex] == INDEX_NONE)
				{
					RegisterController(DeviceIndex);
				}
			}
			// the controller has been disconnected, unmap it 
			else if (DeviceToControllerMap[DeviceIndex] != INDEX_NONE)
			{
				UnregisterController(DeviceIndex);
			}
		}
		break;
		case TrackedDeviceClass_GenericTracker:
		{
			// Check connection status
			if (SteamVRSystem->IsTrackedDeviceConnected(DeviceIndex))
			{
				// has the tracker not been mapped yet
				if (DeviceToControllerMap[DeviceIndex] == INDEX_NONE)
				{
					RegisterTracker(DeviceIndex);
				}
			}
			// the tracker has been disconnected, unmap it 
			else if (DeviceToControllerMap[DeviceIndex] != INDEX_NONE)
			{
				UnregisterTracker(DeviceIndex);
			}
		}
		break;
		case TrackedDeviceClass_Invalid:
			// falls through
		case TrackedDeviceClass_HMD:
			// falls through
		case TrackedDeviceClass_TrackingReference:
			break;
		default:
			UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Encountered unsupported device class of %i!"), (int32)DeviceClass);
			break;
		}
	}
}

bool FSteamVRInputDevice::RegisterController(uint32 DeviceIndex)
{
	// don't map too many controllers
	if (NumControllersMapped >= SteamVRInputDeviceConstants::MaxControllers)
	{
		UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Found more controllers than we support (%i vs %i)!  Probably need to fix this."), NumControllersMapped + 1, SteamVRInputDeviceConstants::MaxControllers);
		return false;
	}

	// Decide which hand to associate this controller with
	EControllerHand ChosenHand = EControllerHand::Special_9;
	{
		ETrackedControllerRole Role = SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex);
		UE_LOG(LogSteamVRInputDevice, Verbose, TEXT("Controller role for device %i is %i (invalid=0, left=1, right=2)."), DeviceIndex, (int32)Role);

		switch (Role)
		{
		case ETrackedControllerRole::TrackedControllerRole_LeftHand:
			ChosenHand = EControllerHand::Left;
			break;
		case ETrackedControllerRole::TrackedControllerRole_RightHand:
			ChosenHand = EControllerHand::Right;
			break;
		case ETrackedControllerRole::TrackedControllerRole_Invalid:
			// falls through
		default:
			return false;
		}
	}

	// determine which player controller to assign the device to
	int32 ControllerIndex = FMath::FloorToInt(NumControllersMapped / CONTROLLERS_PER_PLAYER);

	UE_LOG(LogSteamVRInputDevice, Verbose, TEXT("Controller device %i is being assigned unreal hand %i (left=0, right=1), for player %i."), DeviceIndex, (int32)ChosenHand, ControllerIndex);
	ControllerStates[DeviceIndex].Hand = ChosenHand;
	UnrealControllerHandUsageCount[(int32)ChosenHand] += 1;

	DeviceToControllerMap[DeviceIndex] = ControllerIndex;

	++NumControllersMapped;

	SetUnrealControllerIdToControllerIndex(DeviceToControllerMap[DeviceIndex], ControllerStates[DeviceIndex].Hand, DeviceIndex);

	return true;
}

void FSteamVRInputDevice::DetectHandednessSwap()
{
	const uint32 LeftDeviceIndex = SteamVRSystem->GetTrackedDeviceIndexForControllerRole(TrackedControllerRole_LeftHand);
	const uint32 RightDeviceIndex = SteamVRSystem->GetTrackedDeviceIndexForControllerRole(TrackedControllerRole_RightHand);

	// both hands need to be assigned
	if (LeftDeviceIndex != k_unTrackedDeviceIndexInvalid && RightDeviceIndex != k_unTrackedDeviceIndexInvalid)
	{
		// see if our mappings don't match
		if (ControllerStates[LeftDeviceIndex].Hand != EControllerHand::Left || ControllerStates[RightDeviceIndex].Hand != EControllerHand::Right)
		{
			// explicitly assign the handedness
			ControllerStates[LeftDeviceIndex].Hand = EControllerHand::Left;
			ControllerStates[RightDeviceIndex].Hand = EControllerHand::Right;

			int32 ControllerIndex = DeviceToControllerMap[LeftDeviceIndex];

			SetUnrealControllerIdToControllerIndex(ControllerIndex, EControllerHand::Left, LeftDeviceIndex);
			SetUnrealControllerIdToControllerIndex(ControllerIndex, EControllerHand::Right, RightDeviceIndex);
		}
	}
}

bool FSteamVRInputDevice::RegisterTracker(uint32 DeviceIndex)
{
	// check to see if there are any Special designations left, skip mapping it if there are not
	if (NumTrackersMapped >= SteamVRInputDeviceConstants::MaxSpecialDesignations)
	{
		// go ahead and increment, so we can display a little more info in the log
		++NumTrackersMapped;
		UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Unable to map VR tracker (#%i) to Special hand designation!"), NumTrackersMapped);
		return false;
	}

	// add the tracker to player 0
	DeviceToControllerMap[DeviceIndex] = GENERIC_TRACKER_PLAYER_NUM;

	// select next special #
	switch (NumTrackersMapped)
	{
	case 0:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_1;
		break;
	case 1:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_2;
		break;
	case 2:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_3;
		break;
	case 3:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_4;
		break;
	case 4:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_5;
		break;
	case 5:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_6;
		break;
	case 6:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_7;
		break;
	case 7:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_8;
		break;
	case 8:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_9;
		break;
	case 9:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_10;
		break;
	case 10:
		ControllerStates[DeviceIndex].Hand = EControllerHand::Special_11;
		break;
	default:
		// initial mapping verification above should catch any erroneous NumTrackersMapped
		check(false);
		break;
	}

	++NumTrackersMapped;
	UE_LOG(LogSteamVRInputDevice, Log, TEXT("Tracker device %i is being assigned unreal hand: Special %i, for player %i"), DeviceIndex, NumTrackersMapped, GENERIC_TRACKER_PLAYER_NUM);

	SetUnrealControllerIdToControllerIndex(DeviceToControllerMap[DeviceIndex], ControllerStates[DeviceIndex].Hand, DeviceIndex);

	return true;
}

void FSteamVRInputDevice::UnregisterController(uint32 DeviceIndex)
{
	UnrealControllerHandUsageCount[(int32)ControllerStates[DeviceIndex].Hand] -= 1;
	UnregisterDevice(DeviceIndex);
	NumControllersMapped--;
}

void FSteamVRInputDevice::UnregisterTracker(uint32 DeviceIndex)
{
	UnregisterDevice(DeviceIndex);
	NumTrackersMapped--;
}

void FSteamVRInputDevice::UnregisterDevice(uint32 DeviceIndex)
{
	// undo the mappings
	SetUnrealControllerIdToControllerIndex(DeviceToControllerMap[DeviceIndex], ControllerStates[DeviceIndex].Hand, INDEX_NONE);
	DeviceToControllerMap[DeviceIndex] = INDEX_NONE;

	// re-zero out the controller state
	FMemory::Memzero(&ControllerStates[DeviceIndex], sizeof(FControllerState));
}
#pragma endregion Kept for backwards compatibility and possible future merge back to Engine, note changes however

void FSteamVRInputDevice::InitKnucklesControllerKeys()
{
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_A_CapSense, LOCTEXT("Knuckles_Left_A_CapSense", "SteamVR Knuckles (L) A CapSense"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_A_CapSense, LOCTEXT("Knuckles_Right_A_CapSense", "SteamVR Knuckles (R) A CapSense"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_B_CapSense, LOCTEXT("Knuckles_Left_B_CapSense", "SteamVR Knuckles (L) B CapSense"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_B_CapSense, LOCTEXT("Knuckles_Right_B_CapSense", "SteamVR Knuckles (R) B CapSense"), FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Trigger_CapSense, LOCTEXT("Knuckles_Left_Trigger_CapSense", "SteamVR Knuckles (L) Trigger CapSense"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Trigger_CapSense, LOCTEXT("Knuckles_Right_Trigger_CapSense", "SteamVR Knuckles (R) Trigger CapSense"), FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Thumbstick_CapSense, LOCTEXT("Knuckles_Left_Thumbstick_CapSense", "SteamVR Knuckles (L) Thumbstick CapSense"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Thumbstick_CapSense, LOCTEXT("Knuckles_Right_Thumbstick_CapSense", "SteamVR Knuckles (R) Thumbstick CapSense"), FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Trackpad_CapSense, LOCTEXT("Knuckles_Left_Trackpad_CapSense", "SteamVR Knuckles (L) Trackpad CapSense"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Trackpad_CapSense, LOCTEXT("Knuckles_Right_Trackpad_CapSense", "SteamVR Knuckles (R) Trackpad CapSense"), FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Trackpad_GripForce, LOCTEXT("Knuckles_Left_Trackpad_GripForce", "SteamVR Knuckles (L) Trackpad GripForce"), FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Trackpad_GripForce, LOCTEXT("Knuckles_Right_Trackpad_GripForce", "SteamVR Knuckles (R) Trackpad GripForce"), FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Trackpad_X, LOCTEXT("Knuckles_Left_Trackpad_X", "SteamVR Knuckles (L) Trackpad X"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Trackpad_X, LOCTEXT("Knuckles_Right_Trackpad_X", "SteamVR Knuckles (R) Trackpad X"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Trackpad_Y, LOCTEXT("Knuckles_Left_Trackpad_Y", "SteamVR Knuckles (L) Trackpad Y"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Trackpad_Y, LOCTEXT("Knuckles_Right_Trackpad_Y", "SteamVR Knuckles (R) Trackpad Y"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	// Knuckles Curls
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Finger_Index_Curl, LOCTEXT("Knuckles_Left_Finger_Index_Curl", "SteamVR Knuckles (L) Finger Index Curl"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Finger_Index_Curl, LOCTEXT("Knuckles_Right_Finger_Index_Curl", "SteamVR Knuckles (R) Finger Index Curl"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Finger_Middle_Curl, LOCTEXT("Knuckles_Left_Finger_Middle_Curl", "SteamVR Knuckles (L) Finger Middle Curl"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Finger_Middle_Curl, LOCTEXT("Knuckles_Right_Finger_Middle_Curl", "SteamVR Knuckles (R) Finger Middle Curl"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Finger_Ring_Curl, LOCTEXT("Knuckles_Left_Finger_Ring_Curl", "SteamVR Knuckles (L) Finger Ring Curl"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Finger_Ring_Curl, LOCTEXT("Knuckles_Right_Finger_Ring_Curl", "SteamVR Knuckles (R) Finger Ring Curl"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Finger_Pinky_Curl, LOCTEXT("Knuckles_Left_Finger_Pinky_Curl", "SteamVR Knuckles (L) Finger Pinky Curl"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Finger_Pinky_Curl, LOCTEXT("Knuckles_Right_Finger_Pinky_Curl", "SteamVR Knuckles (R) Finger Pinky Curl"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Finger_Thumb_Curl, LOCTEXT("Knuckles_Left_Finger_Thumb_Curl", "SteamVR Knuckles (L) Finger Thumb Curl"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Finger_Thumb_Curl, LOCTEXT("Knuckles_Right_Finger_Thumb_Curl", "SteamVR Knuckles (R) Finger Thumb Curl"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	// Knuckles Splays
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Finger_ThumbIndex_Splay, LOCTEXT("Knuckles_Left_Finger_ThumbIndex_Splay", "SteamVR Knuckles (L) Finger Thumb-Index Splay"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Finger_ThumbIndex_Splay, LOCTEXT("Knuckles_Right_Finger_ThumbIndex_Splay", "SteamVR Knuckles (R) Finger Thumb-Index Splay"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Finger_IndexMiddle_Splay, LOCTEXT("Knuckles_Left_Finger_IndexMiddle_Splay", "SteamVR Knuckles (L) Finger Index-Middle Splay"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Finger_IndexMiddle_Splay, LOCTEXT("Knuckles_Right_Finger_IndexMiddle_Splay", "SteamVR Knuckles (R) Finger Index-Middle Splay"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Finger_MiddleRing_Splay, LOCTEXT("Knuckles_Left_Finger_MiddleRing_Splay", "SteamVR Knuckles (L) Finger Middle-Ring Splay"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Finger_MiddleRing_Splay, LOCTEXT("Knuckles_Right_Finger_MiddleRing_Splay", "SteamVR Knuckles (R) Finger Middle-Ring Splay"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Left_Finger_RingPinky_Splay, LOCTEXT("Knuckles_Left_Finger_RingPinky_Splay", "SteamVR Knuckles (L) Finger Ring-Pinky Splay"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(KnucklesVRControllerKeys::SteamVR_Knuckles_Right_Finger_RingPinky_Splay, LOCTEXT("Knuckles_Right_Finger_RingPinky_Splay", "SteamVR Knuckles (R) Finger Ring-Pinky Splay"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
}

void FSteamVRInputDevice::GetInputError(EVRInputError InputError, FString InputAction)
{
	// TODO: Refactor strings
	switch (InputError)
	{
	case vr::VRInputError_None:
		UE_LOG(LogSteamVRInputDevice, Display, TEXT("[STEAMVR INPUT ERROR] %s: Success"), *InputAction);
		break;
	case vr::VRInputError_NameNotFound:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Name Not Found"), *InputAction);
		break;
	case vr::VRInputError_WrongType:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Wrong Type"), *InputAction);
		break;
	case vr::VRInputError_InvalidHandle:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Invalid Handle"), *InputAction);
		break;
	case vr::VRInputError_InvalidParam:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Invalid Param"), *InputAction);
		break;
	case vr::VRInputError_NoSteam:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: No Steam"), *InputAction);
		break;
	case vr::VRInputError_MaxCapacityReached:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s:  Max Capacity Reached"), *InputAction);
		break;
	case vr::VRInputError_IPCError:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: IPC Error"), *InputAction);
		break;
	case vr::VRInputError_NoActiveActionSet:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: No Active Action Set"), *InputAction);
		break;
	case vr::VRInputError_InvalidDevice:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Invalid Device"), *InputAction);
		break;
	case vr::VRInputError_InvalidSkeleton:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Invalid Skeleton"), *InputAction);
		break;
	case vr::VRInputError_InvalidBoneCount:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Invalid Bone Count"), *InputAction);
		break;
	case vr::VRInputError_InvalidCompressedData:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Invalid Compressed Data"), *InputAction);
		break;
	case vr::VRInputError_NoData:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: No Data"), *InputAction);
		break;
	case vr::VRInputError_BufferTooSmall:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Buffer Too Small"), *InputAction);
		break;
	case vr::VRInputError_MismatchedActionManifest:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Mismatched Action Manifest"), *InputAction);
		break;
	case vr::VRInputError_MissingSkeletonData:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Missing Skeleton Data"), *InputAction);
		break;
	default:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT ERROR] %s: Unknown Error"), *InputAction);
		break;
	}

	return;
}

#undef LOCTEXT_NAMESPACE //"SteamVRInputDevice"
