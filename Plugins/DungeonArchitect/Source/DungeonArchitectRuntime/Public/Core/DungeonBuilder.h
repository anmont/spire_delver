//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/DungeonModel.h"
#include "Core/DungeonQuery.h"
#include "Core/Utils/PMRandom.h"
#include "Frameworks/ThemeEngine/DungeonThemeAsset.h"

#include "Templates/SubclassOf.h"
#include "DungeonBuilder.generated.h"

class IMarkerGenProcessor;
class FDungeonSceneProvider;
class IDungeonMarkerUserData;
typedef TMap<FString, TArray<FPropTypeData>> PropBySocketType_t;
typedef TMap<UDungeonThemeAsset*, PropBySocketType_t> PropBySocketTypeByTheme_t;

DECLARE_LOG_CATEGORY_EXTERN(DungeonBuilderLog, Log, All);

class ADungeon;
class UDungeonMarkerEmitter;
class ADungeonMirrorVolume;
class UDungeonModel;
class UDungeonConfig;
class UDungeonQuery;
class UDungeonToolData;
class UDungeonSpatialConstraint;
class UProceduralMarkerEmitter;
class UDungeonLevelStreamingModel;
enum class EDungeonMarkerEmitterExecStage : uint8;

/**
*
*/
UCLASS(Blueprintable, abstract)
class DUNGEONARCHITECTRUNTIME_API UDungeonBuilder : public UObject {
    GENERATED_BODY()

public:
    void BuildDungeon(ADungeon* InDungeon, UWorld* InWorld);
    void BuildDungeon(UDungeonModel* InModel, UDungeonConfig* InConfig, UDungeonQuery* InQuery, UWorld* InWorld);
    void DestroyDungeon(UDungeonModel* InModel, UDungeonConfig* InConfig, UDungeonQuery* InQuery, ADungeon* InDungeon, UWorld* InWorld);

    // Non-themed dungeons
    void BuildNonThemedDungeon(ADungeon* InDungeon, TSharedPtr<FDungeonSceneProvider> InSceneProvider, UWorld* InWorld);
    void BuildNonThemedDungeon(UDungeonModel* InModel, UDungeonConfig* InConfig, UDungeonQuery* InQuery, TSharedPtr<FDungeonSceneProvider> InSceneProvider, UWorld* InWorld);
    void DestroyNonThemedDungeon(UDungeonModel* InModel, UDungeonConfig* InConfig, UDungeonQuery* InQuery, ADungeon* InDungeon, UWorld* InWorld);

    void ApplyDungeonTheme(const TArray<UDungeonThemeAsset*>& InThemes, const TArray<FClusterThemeInfo>& InClusteredThemes,
            const TArray<UDungeonMarkerEmitter*>& InMarkerEmitters, TSharedPtr<FDungeonSceneProvider> InSceneProvider, UWorld* InWorld);
    
    virtual void MirrorDungeon();
    virtual void DrawDebugData(UWorld* InWorld, bool bPersistant = false, float LifeTime = -1.0f) {}

    virtual bool SupportsBackgroundTask() const { return true; }
    virtual TSubclassOf<UDungeonModel> GetModelClass() { return nullptr; }
    virtual TSubclassOf<UDungeonConfig> GetConfigClass() { return nullptr; }
    virtual TSubclassOf<UDungeonToolData> GetToolDataClass() { return nullptr; }
    virtual TSubclassOf<UDungeonQuery> GetQueryClass() { return nullptr; }
    virtual bool SupportsProperty(const FName& PropertyName) const { return true; }
    virtual bool SupportsTheming() const { return true; }
    virtual TSharedPtr<FDungeonSceneProvider> CreateSceneProvider(UDungeonConfig* InConfig, ADungeon* InDungeon,
                                                                        UWorld* InWorld);
    virtual bool CanBuildDungeon(FString& OutMessage) { return true; }
    FRandomStream& GetRandomStreamRef() { return Random; }

    UDungeonModel* GetModel() const { return DungeonModel; }

    /** Implementations should process the spatial constraint and return true if it passes */
    virtual bool ProcessSpatialConstraint(UDungeonSpatialConstraint* InSpatialConstraint, const FTransform& InTransform,
                                          FQuat& OutRotationOffset);

    void AddMarker(const FString& SocketType, const FTransform& InTransform, TSharedPtr<IDungeonMarkerUserData> InUserData = nullptr);
    void AddMarker(const FString& InMarkerName, const FTransform& InTransform, int InCount, const FVector& InterOffset, TSharedPtr<IDungeonMarkerUserData> InUserData = nullptr);
    void AddMarker(TArray<FDAMarkerInfo>& pPropSockets, const FString& SocketType, const FTransform& transform, TSharedPtr<IDungeonMarkerUserData> InUserData = nullptr);

    virtual void GetDefaultMarkerNames(TArray<FString>& OutMarkerNames) {
    }

    UFUNCTION(BlueprintNativeEvent, Category = "Dungeon")
    void EmitDungeonMarkers();
    virtual void EmitDungeonMarkers_Implementation();

    UFUNCTION(BlueprintCallable, Category = Dungeon)
    void EmitMarker(const FString& SocketType, const FTransform& Transform);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = Dungeon)
    const TArray<FDAMarkerInfo>& GetMarkers() const { return WorldMarkers; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = Dungeon)
    void GetRandomStream(FRandomStream& OutRandomStream);

    void EmitCustomMarkers(TArray<UDungeonMarkerEmitter*> MarkerEmitters, EDungeonMarkerEmitterExecStage InExecutionStage);
    void EmitProceduralMarkers(const TArray<UProceduralMarkerEmitter*>& InProceduralMarkerEmitters);

    static UClass* DefaultBuilderClass();
    void ProcessMarkerReplacementVolumes();

    virtual bool SupportsLevelStreaming() const { return false; }
    bool HasBuildSucceeded() const { return bBuildSucceeded; }

    virtual void ProcessThemeItemUserData(TSharedPtr<IDungeonMarkerUserData> UserData, AActor* SpawnedActor);
    void ClearMarkerList();

    /** Is this dungeon used for previewing the layout of the dungeon (e.g. in canvas, editor tooling etc) */
    void SetLayoutOnlyPreviewBuild(bool bValue);

protected:
    virtual void BuildDungeonImpl(UWorld* World) { };
    virtual void DestroyDungeonImpl(UWorld* InWorld) { }
    virtual void BuildNonThemedDungeonImpl(UWorld* World, TSharedPtr<class FDungeonSceneProvider> SceneProvider) { };
    virtual void DestroyNonThemedDungeonImpl(UWorld* World) { };
    virtual void MirrorDungeonWithVolume(ADungeonMirrorVolume* MirrorVolume) { }
    virtual bool IdentifyBuildSucceeded() const { return true; }
    virtual TSharedPtr<IMarkerGenProcessor> CreateMarkerGenProcessor(const FTransform& InDungeonTransform) const { return nullptr; }
    virtual bool EmitProceduralMarkers(const UProceduralMarkerEmitter* InProceduralMarkerEmitter) { return false; }
    void ProcessMarkerReplacementVolume(class ADungeonMarkerReplaceVolume* MarkerReplaceVolume);
    UDungeonLevelStreamingModel* GetLevelStreamingModel();
    
    void ClearSockets() {
        _SocketIdCounter = 0;
        WorldMarkers.Reset();
    }

    virtual bool PerformSelectionLogic(const TArray<UDungeonSelectorLogic*>& SelectionLogics, const FDAMarkerInfo& socket) {
        return false;
    }

    virtual FTransform PerformTransformLogic(const TArray<UDungeonTransformLogic*>& TransformLogics, const FDAMarkerInfo& socket) {
        return FTransform::Identity;
    }

    virtual FTransform PerformProceduralTransformLogic(const TArray<UProceduralDungeonTransformLogic*>& ProceduralTransformLogics, const FDAMarkerInfo& InMarkerInfo);

public:
    UPROPERTY()
    UDungeonConfig* DungeonConfig;

protected:
    PMRandom nrandom;
    FRandomStream Random;

    UPROPERTY()
    ADungeon* Dungeon;

    UPROPERTY()
    UDungeonModel* DungeonModel;

    UPROPERTY()
    UDungeonQuery* DungeonQuery;

    int32 _SocketIdCounter;

    bool bBuildSucceeded = true;

    /** Is this dungeon used for previewing the layout of the dungeon (e.g. in canvas, editor tooling etc) */
    bool bLayoutPreviewBuildOnly{};

    // The marker list
    TArray<FDAMarkerInfo> WorldMarkers;
};

