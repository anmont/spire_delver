//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/ThemeEditor/AppModes/ThemeGraph/Graph/EdGraphNode_DungeonMeshList.h"


UObject* UEdGraphNode_DungeonMeshList::GetNodeAssetObject(UObject* Outer) {
	UDungeonMeshList* AssetObject = NewObject<UDungeonMeshList>(Outer);
	AssetObject->StaticMeshes = Meshes;
	AssetObject->ActorTemplates = Blueprints;
	AssetObject->CalculateHashCode();
	return AssetObject;
}

TArray<UObject*> UEdGraphNode_DungeonMeshList::GetThumbnailAssetObjects() {
	TArray<UObject*> ThumbnailObjects;
	for (const FDungeonMeshListItem& MeshInfo : Meshes) {
		ThumbnailObjects.Add(MeshInfo.StaticMesh);
	}
	for (const FDungeonActorTemplateListItem& Blueprint : Blueprints) {
		ThumbnailObjects.Add(Blueprint.ClassTemplate);
	}
	return ThumbnailObjects;
}

FLinearColor UEdGraphNode_DungeonMeshList::GetBorderColor() {
	static constexpr FLinearColor BorderColor(0.2f, 0.12f, 0.08f);
	return BorderColor;
}

