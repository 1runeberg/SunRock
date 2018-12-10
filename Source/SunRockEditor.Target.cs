// Copyright 2018 under the terms of the latest MIT Licence

using UnrealBuildTool;
using System.Collections.Generic;

public class SunRockEditorTarget : TargetRules
{
	public SunRockEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

		ExtraModuleNames.AddRange( new string[] { "SunRock" } );
	}
}
