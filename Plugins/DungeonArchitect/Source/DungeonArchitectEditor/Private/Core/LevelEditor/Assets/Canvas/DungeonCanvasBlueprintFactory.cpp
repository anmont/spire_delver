//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/LevelEditor/Assets/Canvas/DungeonCanvasBlueprintFactory.h"

#include "Builders/Grid/GridDungeonConfig.h"
#include "Core/Common/ContentBrowserMenuExtensions.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditorUtilities.h"
#include "Core/Utils/DungeonEditorViewportProperties.h"
#include "DungeonArchitectEditorModule.h"
#include "Frameworks/Canvas/DungeonCanvas.h"
#include "Frameworks/Canvas/DungeonCanvasBlueprint.h"
#include "Frameworks/Canvas/DungeonCanvasBlueprintGeneratedClass.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"

#define LOCTEXT_NAMESPACE "UDungeonCanvasBlueprintFactory"

UDungeonCanvasBlueprintFactory::UDungeonCanvasBlueprintFactory(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	ParentClass = ADungeonCanvas::StaticClass();
	SupportedClass = UDungeonCanvasBlueprint::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDungeonCanvasBlueprintFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) {
	// Make sure we are trying to factory a Dungeon Canvas Blueprint, then create and init one
	check(InClass->IsChildOf(UDungeonCanvasBlueprint::StaticClass()));

	static const TCHAR* BaseAssetBlueprintClassPath = TEXT("/DungeonArchitect/Core/Features/DungeonCanvas/DefaultDungeonCanvasBase.DefaultDungeonCanvasBase");
	static const TCHAR* BaseAssetActorClassPath = TEXT("/DungeonArchitect/Core/Features/DungeonCanvas/DefaultDungeonCanvasBase.DefaultDungeonCanvasBase_C");
	
	if (ParentClass == ADungeonCanvas::StaticClass()) {
		const TSubclassOf<ADungeonCanvas> BaseActorClass = LoadClass<ADungeonCanvas>(nullptr, BaseAssetActorClassPath);
		UDungeonCanvasBlueprint* BaseBlueprint = LoadObject<UDungeonCanvasBlueprint>(nullptr, BaseAssetBlueprintClassPath);
		if (BaseActorClass && BaseBlueprint) {
			ParentClass = BaseActorClass;
			CanvasBlueprintTemplate = BaseBlueprint;
		}
	}
	
	if (UDungeonCanvasBlueprint* DungeonCanvasBlueprint = CastChecked<UDungeonCanvasBlueprint>(
		FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, InName, BPTYPE_Normal, UDungeonCanvasBlueprint::StaticClass(), UDungeonCanvasBlueprintGeneratedClass::StaticClass(), CallingContext))) {
		if (CanvasBlueprintTemplate.IsValid()) {
			if (UDungeonEditorViewportProperties* TemplateViewportProperties = CanvasBlueprintTemplate->PreviewDungeonProperties) {
				UEngine::CopyPropertiesForUnrelatedObjects(TemplateViewportProperties, DungeonCanvasBlueprint->PreviewDungeonProperties);

				if (TemplateViewportProperties->DungeonConfig) {
					DungeonCanvasBlueprint->PreviewDungeonProperties->DungeonConfig = NewObject<UDungeonConfig>(
						DungeonCanvasBlueprint->PreviewDungeonProperties, TemplateViewportProperties->DungeonConfig->GetClass(),
						NAME_None, RF_NoFlags, TemplateViewportProperties->DungeonConfig);
				}
				else {
					DungeonCanvasBlueprint->PreviewDungeonProperties->DungeonConfig = NewObject<UDungeonConfig>(
						DungeonCanvasBlueprint->PreviewDungeonProperties, UGridDungeonConfig::StaticClass());
				}
			}

			DungeonCanvasBlueprint->MaterialLayers.Reset();
			for (UDungeonCanvasMaterialLayer* TemplateMaterialLayer : CanvasBlueprintTemplate->MaterialLayers) {
				UDungeonCanvasMaterialLayer* MaterialLayer = NewObject<UDungeonCanvasMaterialLayer>(DungeonCanvasBlueprint, NAME_None, RF_NoFlags, TemplateMaterialLayer);
				DungeonCanvasBlueprint->MaterialLayers.Add(MaterialLayer);
			}

			if (!DungeonCanvasBlueprint->MaterialInstance) {
				FDungeonCanvasEditorUtilities::CreateMaterialInstanceInPackage(DungeonCanvasBlueprint);
			}
			
			if (CanvasBlueprintTemplate->MaterialInstance && DungeonCanvasBlueprint->MaterialInstance) {
				DungeonCanvasBlueprint->MaterialInstance->Parent = CanvasBlueprintTemplate->MaterialInstance;
				FDungeonCanvasEditorUtilities::CopyMaterialLayers(CanvasBlueprintTemplate->MaterialInstance, DungeonCanvasBlueprint->MaterialInstance);
			}
		}
		
		DungeonCanvasBlueprint->Modify();
		DungeonCanvasBlueprint->PostLoad();

		/*
		if (UDungeonCanvasBlueprintGeneratedClass* CanvasGeneratedClass = Cast<UDungeonCanvasBlueprintGeneratedClass>(DungeonCanvasBlueprint->GeneratedClass)) {
			CanvasGeneratedClass->MaterialInstance = DungeonCanvasBlueprint->MaterialInstance;
			CanvasGeneratedClass->Modify();
		}
		*/
		
		//FDungeonCanvasEditorUtilities::CompileDungeonCanvasMaterialTemplate(DungeonCanvasBlueprint);
		UMaterialEditingLibrary::UpdateMaterialInstance(DungeonCanvasBlueprint->MaterialInstance);
		DungeonCanvasBlueprint->Status = BS_Dirty;

		return DungeonCanvasBlueprint;
	}
	return nullptr;
}

UObject* UDungeonCanvasBlueprintFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) {
	return FactoryCreateNew(InClass, InParent, InName, Flags, Context, Warn, NAME_None);
}

bool UDungeonCanvasBlueprintFactory::CanCreateNew() const {
	return true;
}

uint32 UDungeonCanvasBlueprintFactory::GetMenuCategories() const {
	return IDungeonArchitectEditorModule::Get().GetDungeonAssetCategoryBit();
}

const TArray<FText>& UDungeonCanvasBlueprintFactory::GetMenuCategorySubMenus() const {
	static const TArray<FText> SubMenus = {
		FDAContentBrowserSubMenuNames::Misc
	};
	return SubMenus;
}

//////////////////////////////// UDungeonCanvasActorFactory ////////////////////////////////

UDungeonCanvasActorFactory::UDungeonCanvasActorFactory(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("DisplayName", "Add Dungeon Canvas Actor");
	NewActorClass = ADungeonCanvas::StaticClass();
}

void UDungeonCanvasActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor) {
	Super::PostSpawnActor(Asset, NewActor);
}

bool UDungeonCanvasActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) {
	if (AssetData.IsValid() && AssetData.GetClass()->IsChildOf(UDungeonCanvasBlueprint::StaticClass())) {
		return true;
	}
	OutErrorMsg = LOCTEXT("CreationErrorMessage", "Invalid Canvas Asset");
	return false;
}

#undef LOCTEXT_NAMESPACE

