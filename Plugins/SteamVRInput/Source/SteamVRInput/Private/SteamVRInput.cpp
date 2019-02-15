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

#include "SteamVRInput.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FSteamVRInputModule"

DEFINE_LOG_CATEGORY_STATIC(LogSteamInput, Log, All);

void FSteamVRInputModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Get the base directory of this plugin
	FString BaseDir = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*IPluginManager::Get().FindPlugin("SteamVRInput")->GetBaseDir());

	// Add on the relative location of the third party dll and load it
	FString OpenVRPath;
#if PLATFORM_WINDOWS
	OpenVRPath = FPaths::Combine(*BaseDir, TEXT("Source/ThirdParty/OpenVRSDK/bin/win64/openvr_api.dll"));
#elif PLATFORM_LINUX
	OpenVRPath = FPaths::Combine(*BaseDir, TEXT("Source/ThirdParty/OpenVRSDK/bin/linux64/libopenvr_api.so"));
#endif // PLATFORM_WINDOWS

	OpenVRHandle = !OpenVRPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*OpenVRPath) : nullptr;

	if (OpenVRHandle)
	{
		UE_LOG(LogSteamInput, Display, TEXT("Latest OpenVR loaded from %s"), *OpenVRPath);
	}
	else
	{
		UE_LOG(LogSteamInput, Error, TEXT("Can't find OpenVR in %s/Source/ThirdParty/OpenVRSDK"), *BaseDir);
	}
}

void FSteamVRInputModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// Free the dll handle
	FPlatformProcess::FreeDllHandle(OpenVRHandle);
	OpenVRHandle = nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSteamVRInputModule, SteamVRInput)
