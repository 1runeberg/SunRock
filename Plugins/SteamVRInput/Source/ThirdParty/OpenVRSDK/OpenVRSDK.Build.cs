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

using System.IO;
using UnrealBuildTool;

public class OpenVRSDK : ModuleRules
{
	public OpenVRSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		string HeadersPath = Path.Combine(ModuleDirectory, "headers");
		string LibraryPath = Path.Combine(ModuleDirectory, "lib");
        string BinaryPath = Path.Combine(ModuleDirectory, "bin");

        // Setup OpenVR Paths based on build platform
        if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(Path.Combine(LibraryPath, "win64"));
			PublicAdditionalLibraries.Add("openvr_api.lib");
			PublicDelayLoadDLLs.Add(Path.Combine(BinaryPath, "win64", "openvr_api.dll"));
            RuntimeDependencies.Add(Path.Combine(BinaryPath, "win64", "openvr_api.dll"));
        }
		else if (Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64"))
		{
			PublicLibraryPaths.Add(Path.Combine(LibraryPath, "linux64"));
			PublicAdditionalLibraries.Add("libopenvr_api.so");
            PublicDelayLoadDLLs.Add(Path.Combine(BinaryPath, "win64", "libopenvr_api.so"));
            RuntimeDependencies.Add(Path.Combine(BinaryPath, "win64", "libopenvr_api.so"));
        }

		// Verify if necessary OpenVR paths exists
		if (!Directory.Exists(HeadersPath) && !Directory.Exists(LibraryPath) && !Directory.Exists(BinaryPath))
		{
			string Err = string.Format("OpenVR SDK not found in {0}", ModuleDirectory);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}

		PublicIncludePaths.Add(HeadersPath);

	}
}
