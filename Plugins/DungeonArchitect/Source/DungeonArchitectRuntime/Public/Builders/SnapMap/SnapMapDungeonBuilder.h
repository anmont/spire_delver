//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Builders/SnapMap/SnapMapDungeonConfig.h"
#include "Core/DungeonBuilder.h"
#include "Core/DungeonModel.h"
#include "Frameworks/Snap/Lib/Streaming/SnapStreaming.h"
#include "Frameworks/ThemeEngine/DungeonThemeAsset.h"
#include "SnapMapDungeonBuilder.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(SnapMapDungeonBuilderLog, Log, All);

class ADungeon;
class USnapMapDungeonConfig;
class USnapConnectionComponent;
class USnapMapDungeonModel;
class FDungeonSceneProvider;
class ASnapConnectionActor;
class ANavMeshBoundsVolume;
class UDungeonLevelStreamingModel;
class UDungeonStreamingChunk;

namespace SnapLib {
    class FDiagnostics;
}

namespace SnapLib {
    typedef TSharedPtr<struct FModuleNode> FModuleNodePtr;
    typedef TSharedPtr<struct FModuleDoor> FModuleDoorPtr;
}


/**
* The snap map builder
*/
UCLASS(Meta=(DisplayName="Snap Map", Description="Get full artistic control on designing the rooms, which are then stitched together using an intuitive Graph Grammar rule editor, offering you complete control on the procedural layout of your dungeons."))
class DUNGEONARCHITECTRUNTIME_API USnapMapDungeonBuilder : public UDungeonBuilder {
    GENERATED_UCLASS_BODY()

public:
    virtual void BuildNonThemedDungeonImpl(UWorld* World, TSharedPtr<FDungeonSceneProvider> SceneProvider) override;
    virtual void DestroyNonThemedDungeonImpl(UWorld* World) override;

    virtual void DrawDebugData(UWorld* InWorld, bool bPersistent = false, float LifeTime = -1.0f) override;
    virtual bool SupportsBackgroundTask() const override { return false; }
    virtual TSubclassOf<UDungeonModel> GetModelClass() override;
    virtual TSubclassOf<UDungeonConfig> GetConfigClass() override;
    virtual TSubclassOf<UDungeonToolData> GetToolDataClass() override;
    virtual TSubclassOf<UDungeonQuery> GetQueryClass() override;
    virtual bool ProcessSpatialConstraint(UDungeonSpatialConstraint* SpatialConstraint, const FTransform& Transform,
                                  FQuat& OutRotationOffset) override;
    virtual bool SupportsProperty(const FName& PropertyName) const override;
    virtual bool SupportsTheming() const override { return false; }
    virtual TSharedPtr<class FDungeonSceneProvider> CreateSceneProvider(UDungeonConfig* Config, ADungeon* pDungeon,
                                                                UWorld* World) override;
    virtual bool CanBuildDungeon(FString& OutMessage) override;
    virtual bool SupportsLevelStreaming() const override { return true; }

    void SetDiagnostics(TSharedPtr<SnapLib::FDiagnostics> InDiagnostics);

    UFUNCTION(BlueprintCallable, Category = Dungeon)
    void BuildPreviewSnapLayout();      // TODO: Get ride of this method

    static void GetSnapConnectionActors(ULevel* ModuleLevel, TArray<ASnapConnectionActor*>& OutConnectionActors);
    
protected:
    virtual bool PerformSelectionLogic(const TArray<UDungeonSelectorLogic*>& SelectionLogics,
                               const FDAMarkerInfo& socket) override;
    virtual FTransform PerformTransformLogic(const TArray<UDungeonTransformLogic*>& TransformLogics,
                                     const FDAMarkerInfo& socket) override;

    SnapLib::FModuleNodePtr GenerateModuleNodeGraph(int32 InSeed) const;
    virtual bool IdentifyBuildSucceeded() const override;
    
protected:
    TWeakObjectPtr<USnapMapDungeonModel> SnapMapModel;
    TWeakObjectPtr<USnapMapDungeonConfig> SnapMapConfig;

    // Optional diagnostics for the dungeon flow editor. Will not be used in standalone builds
    TSharedPtr<SnapLib::FDiagnostics> Diagnostics;
};

