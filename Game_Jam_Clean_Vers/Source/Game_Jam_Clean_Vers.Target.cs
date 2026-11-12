// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Game_Jam_Clean_VersTarget : TargetRules
{
	public Game_Jam_Clean_VersTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V8;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
		ExtraModuleNames.Add("Game_Jam_Clean_Vers");
	}
}
