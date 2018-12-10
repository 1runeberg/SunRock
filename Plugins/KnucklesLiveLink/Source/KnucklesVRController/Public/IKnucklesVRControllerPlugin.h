// COPYRIGHT 1998-2018 EPIC GAMES, INC. ALL RIGHTS RESERVED.
//
// This is a modified version of UE 4.21's
// Engine/Plugins/Runtime/Steam/SteamVR/Source/SteamVRController/Private/SteamVRController.cpp
// Meant as a stopgap until official or more robust support for SteamVR's Input system is available
//
// This input controller will defer/fallback automatically to the official controller if CVar 
// vr.SteamVR.EnableVRInput = 1
//
// Modifs by runeberg.io (Twitter: @1runeberg)

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IInputDeviceModule.h"

#define STEAMVRCONTROLLER_SUPPORTED_PLATFORMS (PLATFORM_MAC || (PLATFORM_LINUX && PLATFORM_CPU_X86_FAMILY && PLATFORM_64BITS) || (PLATFORM_WINDOWS && WINVER > 0x0502))

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class IKnucklesVRControllerPlugin : public IInputDeviceModule
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IKnucklesVRControllerPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IKnucklesVRControllerPlugin >( "KnucklesVRController" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "KnucklesVRController" );
	}
};

