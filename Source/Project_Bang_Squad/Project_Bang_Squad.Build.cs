// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class Project_Bang_Squad : ModuleRules
{
    public Project_Bang_Squad(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "UMG",
            "Slate",
            "SlateCore",
            "OnlineSubsystem",
            "OnlineSubsystemUtils",
            "AIModule",
            "NavigationSystem",
            "GameplayTasks",
            "LevelSequence",
            "MovieScene",
        });
    }
}
