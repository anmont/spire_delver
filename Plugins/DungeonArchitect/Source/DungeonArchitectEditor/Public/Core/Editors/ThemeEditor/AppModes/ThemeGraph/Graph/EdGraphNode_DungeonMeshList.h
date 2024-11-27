//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Actors/DungeonMeshList.h"
#include "Core/Editors/ThemeEditor/AppModes/ThemeGraph/Graph/EdGraphNode_DungeonActorBase.h"
#include "EdGraphNode_DungeonMeshList.generated.h"

UCLASS()
class DUNGEONARCHITECTEDITOR_API UEdGraphNode_DungeonMeshList : public UEdGraphNode_DungeonActorBase {
	GENERATED_BODY()

public:
	virtual UObject* GetNodeAssetObject(UObject* Outer) override;
	virtual TArray<UObject*> GetThumbnailAssetObjects() override;
	virtual FLinearColor GetBorderColor() override;
	
public:
	UPROPERTY(EditAnywhere, Category = Dungeon)
	TArray<FDungeonMeshListItem> Meshes;
	
	UPROPERTY(EditAnywhere, Category = Dungeon)
	TArray<FDungeonActorTemplateListItem> Blueprints;
};

