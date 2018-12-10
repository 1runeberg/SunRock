// Copyright 2018 under the terms of the latest MIT Licence

using UnrealBuildTool;
using System.Collections.Generic;

public class SunRockTarget : TargetRules
{
	public SunRockTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;

		ExtraModuleNames.AddRange( new string[] { "SunRock" } );
	}
}
