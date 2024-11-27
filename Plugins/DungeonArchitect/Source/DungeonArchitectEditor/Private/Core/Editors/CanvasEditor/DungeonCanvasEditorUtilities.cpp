//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/CanvasEditor/DungeonCanvasEditorUtilities.h"

#include "Frameworks/Canvas/DungeonCanvas.h"
#include "Frameworks/Canvas/DungeonCanvasBlueprint.h"
#include "Frameworks/Canvas/DungeonCanvasBlueprintGeneratedClass.h"

#include "MaterialEditingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"

DEFINE_LOG_CATEGORY_STATIC(LogDungeonCanvasEditorUtils, Log, All);

void FDungeonCanvasEditorUtilities::HandleOnAssetAdded(const FAssetData& AssetData) {
	if (const UClass* Class = AssetData.GetClass()) {
		if (Class->IsChildOf(UDungeonCanvasBlueprint::StaticClass())) {
			if (const UDungeonCanvasBlueprint* CanvasBlueprint = Cast<UDungeonCanvasBlueprint>(AssetData.GetAsset())) {
				if (UDungeonCanvasBlueprintGeneratedClass* CanvasGeneratedClass = Cast<UDungeonCanvasBlueprintGeneratedClass>(CanvasBlueprint->GeneratedClass)) {
					if (CanvasGeneratedClass->MaterialInstance != CanvasBlueprint->MaterialInstance) {
						CanvasGeneratedClass->MaterialInstance = CanvasBlueprint->MaterialInstance;
						CanvasGeneratedClass->Modify();
						
						UMaterialEditingLibrary::UpdateMaterialInstance(CanvasBlueprint->MaterialInstance);
						CanvasBlueprint->MaterialInstance->Modify();
					}
				}
			}
		}
	}
}

void FDungeonCanvasEditorUtilities::CompileDungeonCanvasMaterialTemplate(UDungeonCanvasBlueprint* CanvasBlueprint) {
	if (!CanvasBlueprint) {
		return;
	}
	UMaterialInterface* MaterialParent{};
	ADungeonCanvas* CDO = Cast<ADungeonCanvas>(CanvasBlueprint->GeneratedClass->GetDefaultObject());
	if (CDO) {
		MaterialParent = CDO->MaterialTemplateCanvas.LoadSynchronous();
	}

	// Blueprint material
	CanvasBlueprint->Modify();

	if (!CanvasBlueprint->MaterialInstance) {
		CreateMaterialInstanceInPackage(CanvasBlueprint);
	}
	
	CanvasBlueprint->MaterialInstance->Parent = MaterialParent;
	CompileDungeonCanvasMaterialTemplate(CanvasBlueprint->MaterialInstance, CanvasBlueprint->MaterialLayers);

	if (UDungeonCanvasBlueprintGeneratedClass* GeneratedClass = Cast<UDungeonCanvasBlueprintGeneratedClass>(CanvasBlueprint->GeneratedClass)) {
		GeneratedClass->Modify();
		GeneratedClass->MaterialInstance = CanvasBlueprint->MaterialInstance;
	}
}

void FDungeonCanvasEditorUtilities::CompileDungeonCanvasMaterialTemplate(UMaterialInstanceConstant* MaterialInstance, const TArray<UDungeonCanvasMaterialLayer*>& MaterialLayers) {
	if (!MaterialInstance) {
		return;
	}

	FMaterialLayersFunctions LayerFunctions;
	TArray<UDungeonCanvasMaterialLayer*> ReversedMatLayers = MaterialLayers.FilterByPredicate([](const UDungeonCanvasMaterialLayer* InLayer) {
		return InLayer && InLayer->bEnabled;
	});
	Algo::Reverse(ReversedMatLayers);

	int32 LayerIndex = 0;
	for (UDungeonCanvasMaterialLayer* MaterialLayerInfo : ReversedMatLayers) {
		UMaterialFunctionMaterialLayerBlend* MaterialBlend = MaterialLayerInfo->MaterialBlend.LoadSynchronous();
		UMaterialFunctionMaterialLayer* MaterialLayer = MaterialLayerInfo->MaterialLayer.LoadSynchronous();
		
		if (!MaterialLayerInfo || !MaterialLayer || !MaterialBlend) {
			if (MaterialLayerInfo) {
				MaterialLayerInfo->LayerIndex = INDEX_NONE;
			}
			continue;
		}

		if (LayerIndex == 0) {
			LayerFunctions.AddDefaultBackgroundLayer();
			LayerFunctions.Layers[0] = MaterialLayer;
			LayerFunctions.EditorOnly.LayerNames[0] = MaterialLayerInfo->LayerName;
		}
		else {
			const int32 LayerIdx = LayerFunctions.AppendBlendedLayer();
			LayerFunctions.Layers[LayerIdx] = MaterialLayer;
			LayerFunctions.EditorOnly.LayerNames[LayerIdx] = MaterialLayerInfo->LayerName;
			
			check(LayerFunctions.Blends.IsValidIndex(LayerIdx - 1));
			LayerFunctions.Blends[LayerIdx - 1] = MaterialBlend;
		}

		MaterialLayerInfo->LayerIndex = LayerIndex;
		MaterialLayerInfo->Modify();
		
		LayerIndex++;
	}

	if (LayerFunctions.Layers.Num() > 0) {
		MaterialInstance->SetMaterialLayers(LayerFunctions);
		MaterialInstance->Modify();
	}
	
	UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);
}

void FDungeonCanvasEditorUtilities::CreateMaterialInstanceInPackage(UDungeonCanvasBlueprint* CanvasBlueprint) {
	UPackage* AssetPackage = Cast<UPackage>(CanvasBlueprint->GetOuter());
	check(AssetPackage)
	CanvasBlueprint->MaterialInstance = NewObject<UMaterialInstanceConstant>(AssetPackage, UMaterialInstanceConstant::StaticClass(),
		"CompiledMaterial", RF_Standalone | RF_Public);
}

void FDungeonCanvasEditorUtilities::CopyMaterialLayers(const UMaterialInstanceConstant* Source, UMaterialInstanceConstant* Destination) {
	FMaterialLayersFunctions LayerFunctions;
	Source->GetMaterialLayers(LayerFunctions);
	if (LayerFunctions.Layers.Num() > 0) {
		Destination->SetMaterialLayers(LayerFunctions);
		Destination->Modify();
	}
	
	// Copy over the material layer properties
	// Scalar Values:
	{
		TArray<FMaterialParameterInfo> ParamInfoList;
		TArray<FGuid> ParamIds;
		Source->GetAllScalarParameterInfo(ParamInfoList, ParamIds);
		for (const FMaterialParameterInfo& ParamInfo : ParamInfoList) {
			float Value{};
			Source->GetScalarParameterValue(ParamInfo, Value);
			Destination->SetScalarParameterValueEditorOnly(ParamInfo, Value);
		} 
	}
	
	// Vector Values:
	{
		TArray<FMaterialParameterInfo> ParamInfoList;
		TArray<FGuid> ParamIds;
		Source->GetAllVectorParameterInfo(ParamInfoList, ParamIds);
		for (const FMaterialParameterInfo& ParamInfo : ParamInfoList) {
			FLinearColor Value{};
			Source->GetVectorParameterValue(ParamInfo, Value);
			Destination->SetVectorParameterValueEditorOnly(ParamInfo, Value);
		} 
	}
	
	// Texture Values:
	{
		TArray<FMaterialParameterInfo> ParamInfoList;
		TArray<FGuid> ParamIds;
		Source->GetAllTextureParameterInfo(ParamInfoList, ParamIds);
		for (const FMaterialParameterInfo& ParamInfo : ParamInfoList) {
			UTexture* Value{};
			Source->GetTextureParameterValue(ParamInfo, Value);
			Destination->SetTextureParameterValueEditorOnly(ParamInfo, Value);
		} 
	}
}

