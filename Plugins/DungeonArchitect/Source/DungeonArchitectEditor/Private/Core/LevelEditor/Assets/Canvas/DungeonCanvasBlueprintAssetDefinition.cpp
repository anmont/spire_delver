//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/LevelEditor/Assets/Canvas/DungeonCanvasBlueprintAssetDefinition.h"

#include "Core/Editors/CanvasEditor/DungeonCanvasEditor.h"
#include "Core/LevelEditor/Assets/Canvas/DungeonCanvasBlueprintFactory.h"
#include "Frameworks/Canvas/DungeonCanvasBlueprint.h"

#include "Algo/AllOf.h"
#include "AssetDefinitionRegistry.h"
#include "AssetToolsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "DungeonCanvasBlueprintAssetDefinition"

///////////////////////////// UDungeonCanvasBlueprintAssetDefinitionBase /////////////////////////////

FText UDungeonCanvasBlueprintAssetDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("AssetDefinition_DisplayName_DungeonCanvas", "Dungeon Canvas");
}

FLinearColor UDungeonCanvasBlueprintAssetDefinition::GetAssetColor() const
{
	return FLinearColor(FColor(200, 80, 80));
}

TSoftClassPtr<UObject> UDungeonCanvasBlueprintAssetDefinition::GetAssetClass() const
{
	return UDungeonCanvasBlueprint::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UDungeonCanvasBlueprintAssetDefinition::GetAssetCategories() const
{
	static const auto Categories = {EAssetCategoryPaths::Misc};
	return Categories;
}

FAssetSupportResponse UDungeonCanvasBlueprintAssetDefinition::CanLocalize(const FAssetData& InAsset) const
{
	return FAssetSupportResponse::NotSupported();
}


EAssetCommandResult UDungeonCanvasBlueprintAssetDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<FAssetData> OutAssetsThatFailedToLoad;
	for (UDungeonCanvasBlueprint* CanvasBlueprint : OpenArgs.LoadObjects<UDungeonCanvasBlueprint>({}, &OutAssetsThatFailedToLoad))
	{
		bool bLetOpen = true;
		if (!CanvasBlueprint->SkeletonGeneratedClass || !CanvasBlueprint->GeneratedClass)
		{
			bLetOpen = EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("FailedToLoadBlueprintWithContinue", "Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed! Do you want to continue (it can crash the editor)?"));
		}
		if (bLetOpen)
		{
			const TSharedRef<FDungeonCanvasEditor> NewDungeonCanvasEditor = MakeShared<FDungeonCanvasEditor>();
			NewDungeonCanvasEditor->InitDungeonCanvasEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, CanvasBlueprint);
		}
	}

	for (const FAssetData& UnableToLoadAsset : OutAssetsThatFailedToLoad)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("FailedToLoadBlueprint", "Blueprint '{0}' could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!"),
				FText::FromName(UnableToLoadAsset.PackagePath)
			)
		);
	}

	return EAssetCommandResult::Handled;
}


// Menu Extensions
//--------------------------------------------------------------------

namespace MenuExtension_Blueprint
{
	static bool CanExecuteNewDerivedBlueprint(const FToolMenuContext& MenuContext, const FAssetData* SelectedBlueprintPtr)
	{
		const uint32 BPFlags = SelectedBlueprintPtr->GetTagValueRef<uint32>(FBlueprintTags::ClassFlags);
		if ((BPFlags & (CLASS_Deprecated)) == 0)
		{
			return true;
		}

		return false;
	}

	static UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) {
		UDungeonCanvasBlueprintFactory* DungeonCanvasBlueprintFactory = NewObject<UDungeonCanvasBlueprintFactory>();
		DungeonCanvasBlueprintFactory->ParentClass = TSubclassOf<ADungeonCanvas>(*InBlueprint->GeneratedClass);
		if (const UDungeonCanvasBlueprint* CanvasBlueprint = CastChecked<UDungeonCanvasBlueprint>(InBlueprint)) {
			DungeonCanvasBlueprintFactory->CanvasBlueprintTemplate = CanvasBlueprint;
		}
	
		return DungeonCanvasBlueprintFactory;
	}

	static void ExecuteNewDerivedBlueprint(const FToolMenuContext& MenuContext, const FAssetData* SelectedBlueprintPtr)
	{
		if (UBlueprint* ParentBlueprint = Cast<UBlueprint>(SelectedBlueprintPtr->GetAsset()))
		{
			UClass* TargetParentClass = ParentBlueprint->GeneratedClass;

			if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(TargetParentClass))
			{
				FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("InvalidClassToMakeBlueprintFrom", "Invalid class with which to make a Blueprint."));
				return;
			}

			FString Name;
			FString PackageName;
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			AssetToolsModule.Get().CreateUniqueAssetName(ParentBlueprint->GetOutermost()->GetName(), TEXT("_Child"), PackageName, Name);
			const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

			UFactory* Factory = GetFactoryForBlueprintType(ParentBlueprint);
			const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, PackagePath, ParentBlueprint->GetClass(), Factory);
		}
	}

	static void ExecuteEditDefaults(const FToolMenuContext& MenuContext, TArray<FAssetData> BlueprintAssets)
	{
		TArray<UBlueprint*> Blueprints;

		FMessageLog EditorErrors("EditorErrors");
		EditorErrors.NewPage(LOCTEXT("ExecuteEditDefaultsNewLogPage", "Loading Blueprints"));

		for (const FAssetData& BlueprintAsset : BlueprintAssets)
		{
			if (UBlueprint* Object = Cast<UBlueprint>(BlueprintAsset.GetAsset()))
			{
				// If the blueprint is valid, allow it to be added to the list, otherwise log the error.
				if ( Object->SkeletonGeneratedClass && Object->GeneratedClass )
				{
					Blueprints.Add(Object);
				}
				else
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("ObjectName"), FText::FromString(Object->GetName()));
					EditorErrors.Error(FText::Format(LOCTEXT("LoadBlueprint_FailedLog", "{ObjectName} could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!"), Arguments ) );
				}
			}
		}

		if ( Blueprints.Num() > 0 )
		{
			FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>( "Kismet" );
			TSharedRef< IBlueprintEditor > NewBlueprintEditor = BlueprintEditorModule.CreateBlueprintEditor(  EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), Blueprints );
		}

		// Report errors
		EditorErrors.Notify(LOCTEXT("OpenDefaults_Failed", "Opening Class Defaults Failed!"));
	}
	
	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []{ 
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UDungeonCanvasBlueprint::StaticClass());
	        
			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
				Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
				{
					if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection))
					{
						if (const FAssetData* SelectedBlueprintPtr = Context->GetSingleSelectedAssetOfType(UDungeonCanvasBlueprint::StaticClass(), EIncludeSubclasses::Yes))
						{
							const TAttribute<FText> Label = LOCTEXT("Blueprint_NewDerivedBlueprint", "Create Child Canvas Blueprint Class");
							const TAttribute<FText> ToolTip = TAttribute<FText>::CreateLambda([SelectedBlueprintPtr]()
							{
								const uint32 BPFlags = SelectedBlueprintPtr->GetTagValueRef<uint32>(FBlueprintTags::ClassFlags);
								if ((BPFlags & (CLASS_Deprecated)) == 0)
								{
									return LOCTEXT("Blueprint_NewDerivedBlueprintTooltip", "Creates a Child Blueprint Class based on the current Blueprint, allowing you to create variants easily.");
								}
								else
								{
									return LOCTEXT("Blueprint_NewDerivedBlueprintIsDeprecatedTooltip", "Blueprint class is deprecated, cannot derive a child Blueprint!");
								}
							});
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Blueprint");

							FToolUIAction DeriveNewBlueprint;
							DeriveNewBlueprint.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteNewDerivedBlueprint, SelectedBlueprintPtr);
							DeriveNewBlueprint.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanExecuteNewDerivedBlueprint, SelectedBlueprintPtr);
							InSection.AddMenuEntry("CreateChildBlueprintClass", Label, ToolTip, Icon, DeriveNewBlueprint);
						}

						TArray<FAssetData> SelectedBlueprints = Context->GetSelectedAssetsOfType(UDungeonCanvasBlueprint::StaticClass(), EIncludeSubclasses::No);
						if (SelectedBlueprints.Num() > 1)
						{
							TArray<UClass*> SelectedBlueprintParentClasses;
							Algo::Transform(SelectedBlueprints, SelectedBlueprintParentClasses, [](const FAssetData& BlueprintAsset){ return UDungeonCanvasBlueprint::GetBlueprintParentClassFromAssetTags(BlueprintAsset); });
							
							// Ensure that all the selected blueprints are actors
							const bool bAreAllSelectedBlueprintsActors =
								Algo::AllOf(SelectedBlueprintParentClasses, [](UClass* ParentClass){ return ParentClass->IsChildOf(AActor::StaticClass()); });
							
							if (bAreAllSelectedBlueprintsActors)
							{
								const TAttribute<FText> Label = LOCTEXT("Blueprint_EditDefaults", "Edit Shared Defaults");
								const TAttribute<FText> ToolTip = LOCTEXT("Blueprint_EditDefaultsTooltip", "Edit the shared default properties of the selected actor blueprints.");
								const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.Tabs.BlueprintDefaults");
								const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&ExecuteEditDefaults, MoveTemp(SelectedBlueprints));
								InSection.AddMenuEntry("Blueprint_EditDefaults", Label, ToolTip, Icon, UIAction);
							}
						}
					}
				}));
		}));
	});
}


#undef LOCTEXT_NAMESPACE

