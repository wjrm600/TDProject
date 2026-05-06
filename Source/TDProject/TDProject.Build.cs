// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TDProject : ModuleRules
{
	public TDProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			// Phase 3A: 네트워킹 모듈 (세션/로비/데디서버)
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			// GAS Phase 0: Gameplay Ability System 모듈
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"TDProject",
			"TDProject/AOS",
			"TDProject/AOS/AI",
			"TDProject/AOS/GAS",
			"TDProject/AOS/GAS/Abilities",
			"TDProject/AOS/GAS/Data",
			"TDProject/AOS/GAS/Effects",
			"TDProject/AOS/UI",
			"TDProject/Variant_Platforming",
			"TDProject/Variant_Platforming/Animation",
			"TDProject/Variant_Combat",
			"TDProject/Variant_Combat/AI",
			"TDProject/Variant_Combat/Animation",
			"TDProject/Variant_Combat/Gameplay",
			"TDProject/Variant_Combat/Interfaces",
			"TDProject/Variant_Combat/UI",
			"TDProject/Variant_SideScrolling",
			"TDProject/Variant_SideScrolling/AI",
			"TDProject/Variant_SideScrolling/Gameplay",
			"TDProject/Variant_SideScrolling/Interfaces",
			"TDProject/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
