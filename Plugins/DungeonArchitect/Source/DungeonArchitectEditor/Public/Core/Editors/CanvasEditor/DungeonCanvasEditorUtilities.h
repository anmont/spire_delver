//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"

class UDungeonCanvasMaterialLayer;
class UDungeonCanvasBlueprint;
class UMaterialInstanceConstant;

class FDungeonCanvasEditorUtilities {
public:
	static void HandleOnAssetAdded(const FAssetData& AssetData);
	static void CopyMaterialLayers(const UMaterialInstanceConstant* Source, UMaterialInstanceConstant* Destination);
	
	static void CompileDungeonCanvasMaterialTemplate(UDungeonCanvasBlueprint* CanvasBlueprint);
	static void CompileDungeonCanvasMaterialTemplate(UMaterialInstanceConstant* MaterialInstance, const TArray<UDungeonCanvasMaterialLayer*>& MaterialLayers);

	static void CreateMaterialInstanceInPackage(UDungeonCanvasBlueprint* CanvasBlueprint);
};

