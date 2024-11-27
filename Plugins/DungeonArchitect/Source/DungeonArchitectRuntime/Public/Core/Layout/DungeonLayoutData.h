//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Layout/FloorSettings.h"
#include "Core/Utils/DungeonBoundingShapes.h"
#include "Core/Utils/PointOfInterest.h"
#include "Frameworks/Canvas/DungeonCanvasRoomShapeTexture.h"
#include "DungeonLayoutData.generated.h"

USTRUCT()
struct DUNGEONARCHITECTRUNTIME_API FDungeonLayoutDataChunkInfo {
    GENERATED_BODY()

    UPROPERTY()
    TArray<FDABoundsShapeCircle> Circles;
    
    UPROPERTY()
    TArray<FDABoundsShapeConvexPoly> ConvexPolys;
    
    UPROPERTY()
    TArray<FDABoundsShapeLine> Outlines;
    
    UPROPERTY()
    TArray<FDungeonCanvasRoomShapeTexture> CanvasShapeTextures;

    UPROPERTY()
    TArray<FDungeonPointOfInterest> PointsOfInterest;
    
    void Append(const FDungeonLayoutDataChunkInfo& Other) {
        Circles.Append(Other.Circles);
        ConvexPolys.Append(Other.ConvexPolys);
        CanvasShapeTextures.Append(Other.CanvasShapeTextures);
        PointsOfInterest.Append(Other.PointsOfInterest);
        Outlines.Append(Other.Outlines);
    }
    
    void Append(const FDungeonLayoutDataChunkInfo& Other, const TArray<FName>& TagsToIgnore) {
        auto ShouldIgnore = [&TagsToIgnore](const TArray<FName>& ShapeTags) {
            for (const FName& IgnoreTag : TagsToIgnore) {
                if (ShapeTags.Contains(IgnoreTag)) {
                    return true;
                }
            }
            return false;
        };
        
        for (const FDABoundsShapeCircle& OtherShape : Other.Circles) {
            if (!ShouldIgnore(OtherShape.Tags)) {
                Circles.Add(OtherShape);
            }
        }
        for (const FDABoundsShapeConvexPoly& OtherShape : Other.ConvexPolys) {
            if (!ShouldIgnore(OtherShape.Tags)) {
                ConvexPolys.Add(OtherShape);
            }
        }
        for (const FDungeonCanvasRoomShapeTexture& OtherShape : Other.CanvasShapeTextures) {
            if (!ShouldIgnore(OtherShape.Tags)) {
                CanvasShapeTextures.Add(OtherShape);
            }
        }
        for (const FDABoundsShapeLine& OtherShape : Other.Outlines) {
            if (!ShouldIgnore(OtherShape.Tags)) {
                Outlines.Add(OtherShape);
            }
        }

        // No need to filter points of interest by tags
        PointsOfInterest.Append(Other.PointsOfInterest);
    }
};

USTRUCT()
struct DUNGEONARCHITECTRUNTIME_API FDungeonLayoutDataDoorItem {
    GENERATED_BODY()
    
    UPROPERTY()
    FTransform WorldTransform = FTransform::Identity;

    UPROPERTY()
    float Width = 400;

    /**
     * The layout is initially rendered without doors. Then the walls are removed to make way for doors, where ever it's needed.
     * This value controls how thick our wall removal brush is.
     * If you don't see your walls getting removed properly, try increasing it a bit
     */
    UPROPERTY()
    float DoorOcclusionThickness = 300;
    
    UPROPERTY()
    TArray<FVector> Outline_DEPRECATED;
};

USTRUCT()
struct DUNGEONARCHITECTRUNTIME_API FDungeonLayoutDataStairItem {
    GENERATED_BODY()
    
    UPROPERTY()
    FTransform WorldTransform = FTransform::Identity;

    UPROPERTY()
    float Width = 400;
};

USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDungeonLayoutData {
    GENERATED_BODY()

    UPROPERTY()
    TArray<FDungeonLayoutDataChunkInfo> ChunkShapes;
    
    UPROPERTY()
    TArray<FDungeonLayoutDataDoorItem> Doors;

    UPROPERTY()
    TArray<FDungeonLayoutDataStairItem> Stairs;
    
    UPROPERTY()
    TArray<FDungeonPointOfInterest> PointsOfInterest;

    UPROPERTY()
    FDungeonFloorSettings FloorSettings;

    UPROPERTY(BlueprintReadOnly, Category="Dungeon Architect")
    FBox Bounds = FBox(ForceInitToZero);
};

class DUNGEONARCHITECTRUNTIME_API FDungeonLayoutUtils {
public:
    struct FCalcBoundsSettings {
        bool bIncludeTextureOverlays;
        FCalcBoundsSettings()
            : bIncludeTextureOverlays(false)
        {
        }
    };
    
    static FBox CalculateBounds(const FDungeonLayoutDataChunkInfo& LayoutShapes, const FCalcBoundsSettings& Settings = {});
    static FDungeonLayoutData FilterByHeightRange(const FDungeonLayoutData& InLayoutData, float MinHeight, float MaxHeight);
};


