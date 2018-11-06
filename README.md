# SUN ROCK (early release alpha)
**UE4 LIVELINK &amp; INPUT SYSTEM PLUGIN FOR STEAMVR KNUCKLES**

Includes a Livelink Plugin for SteamVR's new Knuckles Streaming Hand Skeletal Data System and a hook to the new Action Binding System to UE4's native Event System.

This demo project only works with UE4.21, SteamVR EV2 & EV3 Knuckles Hardware **AND** Steam **MUST** be running.

**NOTE 1: If you are using a custom 4.21 Build (i.e. from Source rather than from the Launcher), use the following instructions to ensure the Binary plugins here work for your Source Built Engine:**

1. Go to 'Plugins > KnucklesLiveLink > Binaries > Win64' 
2. Edit the 'UE4Editor.modules' file. 
3. Change the BuildID to match your UE4 editor BuildID and it'll load (find this by looking in 'Engine > Binaries > Win64 > UE4Editor.modules')

Workaround courtesy of: https://steamcommunity.com/id/itsnotmetrustme

**NOTE 2: To reset Action Manifest for UE4Editor to defaults:**

1. Go to "Program Files (x86)\Steam\config\steamvr.settings"
2. In there, will have a section called "system.generated.ue4editor.exe"
3. Delete this section to restore defaults 

Courtesy of: https://steamcommunity.com/id/lamboman007

## Preview Video:
https://youtu.be/0Z49S7Q5lpw

[![KNUCKLES UE4 PLUGIN](http://img.youtube.com/vi/DDu5W_b88N0/0.jpg)](http://www.youtube.com/watch?v=DDu5W_b88N0 "Knuckles UE4 Plugin Overview")

## Demo Project Requirements

1. Unreal Engine 4.21
2. Knuckles Hardware (EV2 or EV3)
3. SteamVR Beta
4. Steam Running and User Logged-In
5. Knuckles Livelink & Input System Plugin (binary included in this repo)
6. Pre-release 3.0.6 alpha version of RunebergVR Plugin (binary included and enabled in this repo)
7. Livelink Engine Plugin from Epic (included in 4.21, enabled in this repo)
8. SteamVR Engine Plugin (included in 4.21, enabled by default)
