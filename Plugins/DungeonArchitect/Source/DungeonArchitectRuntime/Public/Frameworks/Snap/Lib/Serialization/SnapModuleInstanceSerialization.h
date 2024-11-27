//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Layout/DungeonLayoutData.h"
#include "Core/Utils/DungeonBoundingShapes.h"
#include "Frameworks/Canvas/DungeonCanvasRoomShapeTexture.h"
#include "SnapModuleInstanceSerialization.generated.h"

USTRUCT()
struct DUNGEONARCHITECTRUNTIME_API FSnapModuleInstanceSerializedData {
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FGuid ModuleInstanceId;

	UPROPERTY()
	FTransform WorldTransform;

	UPROPERTY()
	TSoftObjectPtr<UWorld> Level;

	UPROPERTY()
	TMap<FName, TSoftObjectPtr<UWorld>> ThemedLevels;
    
	UPROPERTY()
	FName Category;

	UPROPERTY()
	FBox ModuleBounds = FBox(ForceInitToZero);
    
	UPROPERTY()
	FDABoundsShapeList ModuleBoundShapes;

	UPROPERTY()
	FDungeonCanvasRoomShapeTextureList CanvasRoomShapeTextures;

	UPROPERTY()
	TArray<FDungeonPointOfInterest> PointsOfInterest;
    
	TSoftObjectPtr<UWorld> GetThemedLevel(const FName& InThemeName) const;
};

