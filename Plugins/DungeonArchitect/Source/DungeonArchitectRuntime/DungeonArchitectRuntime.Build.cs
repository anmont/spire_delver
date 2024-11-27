//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

namespace UnrealBuildTool.Rules
{
	public class DungeonArchitectRuntime : ModuleRules
	{
		public DungeonArchitectRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			bLegacyPublicIncludePaths = false;
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			ShadowVariableWarningLevel = WarningLevel.Error;
			IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
            
			PublicIncludePaths.AddRange(new string[] {
				// ... add public include paths required here ...
			});

			PublicDependencyModuleNames.AddRange(new string[] {
				// ... add other public dependencies that you statically link with here ...
			});

			PrivateDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Foliage",
				"RenderCore",
				"PhysicsCore",
				"GeometryCore",
				"GeometryFramework",
				"Landscape",
				"AssetRegistry",
				"NavigationSystem",
				"AIModule",
				"RHI",
				"UMG",
				"Slate",
				"SlateCore",
				"InputCore",
				"Projects",
			});

			DynamicallyLoadedModuleNames.AddRange(new string[] {
				// ... add any modules that your module loads dynamically here ...
			});
		}
	}
}
