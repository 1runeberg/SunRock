// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class KnucklesVRController : ModuleRules
{
    public KnucklesVRController(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Public/IKnucklesVRControllerPlugin.h";

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PrivateIncludePathModuleNames.AddRange(new string[]
		{
			"TargetPlatform",
            "SteamVR"
		});

        PrivateDependencyModuleNames.AddRange(new string[]
        {
			"Core",
			"CoreUObject",
			"ApplicationCore",
			"Engine",
			"InputDevice",
            "InputCore",
			"HeadMountedDisplay",
            "SteamVR",
            "Json",
            "JsonUtilities",
            "KnucklesLiveLink"
        });

        // 		DynamicallyLoadedModuleNames.AddRange(new string[]
        // 		{
        // 			"SteamVR",
        // 		});

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }

        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenVR");

        if ( Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || (Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64")) )
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
            PrivateDependencyModuleNames.Add("OpenGLDrv");
        }
    }
}
