// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class KnucklesLiveLink : ModuleRules
{
	public KnucklesLiveLink(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "OpenVR",
                "LiveLinkInterface",
                "InputDevice",
                "InputCore",
                "HeadMountedDisplay",
                "SteamVR",
                "Json"
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore"
            }
			);

    }
}
