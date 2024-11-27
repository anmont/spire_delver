//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

namespace UnrealBuildTool.Rules
{
    public class DungeonArchitectEditor : ModuleRules
    {
        public DungeonArchitectEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            bLegacyPublicIncludePaths = false;
            PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
            ShadowVariableWarningLevel = WarningLevel.Error;
            IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
            
            PublicIncludePaths.AddRange(new string[] {
                    // ... add public include paths required here ...
            });

            PrivateIncludePaths.AddRange(new[] {
                "DungeonArchitectEditor/Private"
            });

            PrivateIncludePathModuleNames.AddRange(new[] {
                "Settings",
                "MessageLog"
            });

            PublicDependencyModuleNames.AddRange(new string[]
            {
            });

            PrivateDependencyModuleNames.AddRange(new[] {
                "DungeonArchitectRuntime",
                
                "AddContentDialog",
                "AdvancedPreviewScene",
                "ApplicationCore",
                "AppFramework",
                "AssetRegistry",
                "AssetDefinition",
                "BlueprintGraph",
                "ContentBrowser",
                "Core",
                "CoreUObject",
                "EditorFramework",
                "EditorInteractiveToolsFramework",
                "EditorScriptingUtilities",
                "EditorStyle",
                "EditorWidgets",
                "Engine",
                "Foliage",
                "GeometryFramework",
                "GeometryScriptingCore",
                "GeometryScriptingEditor",
                "GraphEditor",
                "InputCore",
                "InteractiveToolsFramework",
                "Json",
                "JsonUtilities",
                "Kismet",
                "KismetCompiler",
                "KismetWidgets",
                "LevelEditor",
                "MainFrame",
                "MaterialEditor",
                "MaterialUtilities",
                "PlacementMode",
                "Projects",
                "PropertyEditor",
                "RenderCore",
                "RHI",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "ToolWidgets",
                "UnrealEd",
                "AssetTools",
                "WorkspaceMenuStructure",
                "ToolPresetEditor",
                "ToolPresetAsset", 
                "ModelingEditorUI",
                "ModelingComponents",
                "ModelingComponentsEditorOnly",
                "WidgetRegistration",
                "EditorConfig",
                "DeveloperSettings",
                "StatusBar",
                "ToolWidgets",
            });

            DynamicallyLoadedModuleNames.AddRange(new string[] {
                // ... add any modules that your module loads dynamically here ...
            });
        }
    }
}
