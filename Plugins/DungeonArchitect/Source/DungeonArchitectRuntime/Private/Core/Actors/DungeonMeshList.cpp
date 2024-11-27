//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Actors/DungeonMeshList.h"

#include "Engine/StaticMesh.h"

void UDungeonMeshList::CalculateHashCode() {
	FString HashData;
	for (const FDungeonMeshListItem& Item : StaticMeshes) {
		if (!Item.StaticMesh) continue;
		
		HashData += Item.StaticMesh->GetFullName();
		HashData += "|";
	}
	
	for (const FDungeonActorTemplateListItem& Item : ActorTemplates) {
		if (!Item.ClassTemplate) continue;
		
		HashData += Item.ClassTemplate->GetFullName();
		HashData += "|";
	}
	
	HashCode = GetTypeHash(HashData);
	
}

