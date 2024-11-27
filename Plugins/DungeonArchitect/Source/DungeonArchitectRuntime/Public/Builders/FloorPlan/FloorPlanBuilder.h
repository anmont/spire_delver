//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Builders/FloorPlan/FloorPlanModel.h"
#include "Core/DungeonBuilder.h"
#include "FloorPlanBuilder.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(FloorPlanBuilderLog, Log, All);

class ADungeon;
class ADungeonVolume;
class UFloorPlanConfig;

/**
*
*/
UCLASS(Experimental, Meta=(DisplayName="Floor Plan", Description="A work-in-progress builder that generates indoor building floorplans. This builder is not feature complete yet"))
class DUNGEONARCHITECTRUNTIME_API UFloorPlanBuilder : public UDungeonBuilder {
    GENERATED_BODY()

public:
    virtual void BuildDungeonImpl(UWorld* World) override;
    virtual void EmitDungeonMarkers_Implementation() override;
    virtual void DrawDebugData(UWorld* InWorld, bool bPersistant = false, float lifeTime = -1.0f) override;
    virtual bool SupportsBackgroundTask() const override { return false; }
    virtual TSubclassOf<UDungeonModel> GetModelClass() override;
    virtual TSubclassOf<UDungeonConfig> GetConfigClass() override;
    virtual TSubclassOf<UDungeonToolData> GetToolDataClass() override;
    virtual TSubclassOf<UDungeonQuery> GetQueryClass() override;
    virtual bool ProcessSpatialConstraint(UDungeonSpatialConstraint* SpatialConstraint, const FTransform& Transform,
                                          FQuat& OutRotationOffset) override;
    virtual void GetDefaultMarkerNames(TArray<FString>& OutMarkerNames) override;

protected:
    virtual bool PerformSelectionLogic(const TArray<UDungeonSelectorLogic*>& SelectionLogics,
                                       const FDAMarkerInfo& socket) override;
    virtual FTransform PerformTransformLogic(const TArray<UDungeonTransformLogic*>& TransformLogics,
                                             const FDAMarkerInfo& socket) override;
    virtual TSharedPtr<IMarkerGenProcessor> CreateMarkerGenProcessor(const FTransform& InDungeonTransform) const override;

private:
    void BuildLayout();
    void SplitChunk(FloorChunkPtr Chunk, FloorChunkPtr OutLeft, FloorChunkPtr OutRight, FloorChunkPtr OutHallway);
    void SplitChunk(FloorChunkPtr Chunk, FloorChunkPtr OutLeft, FloorChunkPtr OutRight);
    void EmitMarkerAt(const FVector& WorldLocation, const FString& MarkerName, const FQuat& Rotation);
    void EmitMarkerAt(const FVector& WorldLocation, const FString& MarkerName, float Angle);
    void EmitSeparatorMarkers(const FString& WallMarkerName, const FString& SepratorMarkerName,
                              const FVector& GridSize);
    bool VolumeEncompassesPoint(ADungeonVolume* Volume, const FIntVector& GridPoint);
    void GetVolumeCells(ADungeonVolume* Volume, int32 z, TArray<FIntVector>& OutCells);
    void CreateDoors(int z);

private:
    UPROPERTY(Transient)
    UFloorPlanModel* FloorPlanModel = {};
    
    UPROPERTY(Transient)
    UFloorPlanConfig* FloorPlanConfig = {};
    
    FloorChunkDBPtr ChunkDB;
    FloorDoorManager DoorManager;
    FRandomStream Random;

    TSet<FloorChunkId> Visited;
};

