// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Game_Jam_Clean_Vers : ModuleRules
{
	public Game_Jam_Clean_Vers(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
            "Niagara",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "ImageWrapper", "RenderCore", "RHI", "Json" });
		if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "MeshDescription", "StaticMeshDescription", "SkeletalMeshDescription", "AssetRegistry", "Json" });

		PublicIncludePaths.AddRange(new string[] {
			"Game_Jam_Clean_Vers",
			"Game_Jam_Clean_Vers/Variant_Horror",
			"Game_Jam_Clean_Vers/Variant_Horror/UI",
			"Game_Jam_Clean_Vers/Variant_Shooter",
			"Game_Jam_Clean_Vers/Variant_Shooter/AI",
			"Game_Jam_Clean_Vers/Variant_Shooter/UI",
			"Game_Jam_Clean_Vers/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
