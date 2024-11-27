//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/DungeonBuilder.h"

#include "Builders/Grid/GridDungeonBuilder.h"
#include "Core/Dungeon.h"
#include "Core/DungeonConfig.h"
#include "Core/DungeonEventListener.h"
#include "Core/DungeonQuery.h"
#include "Core/Utils/Debug/SceneDebugDataComponent.h"
#include "Core/Utils/DungeonModelHelper.h"
#include "Core/Volumes/DungeonMarkerReplaceVolume.h"
#include "Core/Volumes/DungeonMirrorVolume.h"
#include "Core/Volumes/DungeonThemeOverrideVolume.h"
#include "Frameworks/ThemeEngine/DungeonThemeEngine.h"
#include "Frameworks/ThemeEngine/Markers/DungeonMarkerEmitter.h"
#include "Frameworks/ThemeEngine/Markers/ProceduralMarkers/ProceduralMarkerEmitter.h"
#include "Frameworks/ThemeEngine/Rules/Transformer/ProceduralDungeonTransformLogic.h"
#include "Frameworks/ThemeEngine/SceneProviders/InstancedDungeonSceneProvider.h"
#include "Frameworks/ThemeEngine/SceneProviders/PooledDungeonSceneProvider.h"

#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY(DungeonBuilderLog);


void CreatePropLookup(UDungeonThemeAsset* PropAsset, PropBySocketTypeByTheme_t& PropBySocketTypeByTheme) {
    if (!PropAsset || PropBySocketTypeByTheme.Contains(PropAsset)) {
        // Lookup for this theme has already been built
        return;
    }

    PropBySocketTypeByTheme.Add(PropAsset, PropBySocketType_t());
    PropBySocketType_t& PropBySocketType = PropBySocketTypeByTheme[PropAsset];

    for (const FPropTypeData& Prop : PropAsset->Props) {
        if (!PropBySocketType.Contains(Prop.AttachToSocket)) {
            PropBySocketType.Add(Prop.AttachToSocket, TArray<FPropTypeData>());
        }
        PropBySocketType[Prop.AttachToSocket].Add(Prop);
    }
}

// Picks a theme from the list that has a definition for the defined socket
UDungeonThemeAsset* GetBestMatchedTheme(const FRandomStream& random, const TArray<UDungeonThemeAsset*>& Themes,
                                        const FDAMarkerInfo& socket, PropBySocketTypeByTheme_t& PropBySocketTypeByTheme) {
    TArray<UDungeonThemeAsset*> ValidThemes;
    for (UDungeonThemeAsset* Theme : Themes) {
        if (PropBySocketTypeByTheme.Contains(Theme)) {
            PropBySocketType_t& PropBySocketType = PropBySocketTypeByTheme[Theme];
            if (PropBySocketType.Num() > 0) {
                if (PropBySocketType.Contains(socket.MarkerName) && PropBySocketType[socket.MarkerName].Num() > 0) {
                    ValidThemes.Add(Theme);
                }
            }
        }
    }
    if (ValidThemes.Num() == 0) {
        return nullptr;
    }

    int32 index = FMath::FloorToInt(random.FRand() * ValidThemes.Num()) % ValidThemes.Num();
    return ValidThemes[index];
}

struct ThemeOverrideInfo {
    FRectangle Bounds;
    ADungeonThemeOverrideVolume* Volume;
};

void GenerateThemeOverrideList(UWorld* World, ADungeon* Dungeon, PropBySocketTypeByTheme_t& PropBySocketTypeByTheme,
                               TArray<ThemeOverrideInfo>& OutOverrideList) {
    if (!World) return;

    OutOverrideList.Reset();
    if (World) {
        for (TActorIterator<ADungeonThemeOverrideVolume> VolumeIt(World); VolumeIt; ++VolumeIt) {
            ADungeonThemeOverrideVolume* Volume = *VolumeIt;
            if (!IsValid(Volume) || !Volume->IsValidLowLevel()) {
                continue;
            }
            bool valid;
            if (!Dungeon) {
                valid = true;
            }
            else {
                valid = (Volume->Dungeon == Dungeon);
            }
            if (valid && Volume->ThemeOverride) {
                FRectangle VolumeBounds;
                Volume->GetDungeonVolumeBounds(FVector(1, 1, 1), VolumeBounds);
                ThemeOverrideInfo Info;
                Info.Bounds = VolumeBounds;
                Info.Volume = Volume;
                OutOverrideList.Add(Info);

                // Build a lookup of the theme for faster access later on
                CreatePropLookup(Volume->ThemeOverride, PropBySocketTypeByTheme);
            }
        }
    }
}


void UDungeonBuilder::BuildDungeon(ADungeon* InDungeon, UWorld* InWorld) {
    this->Dungeon = InDungeon;
    if (!Dungeon) {
        UE_LOG(DungeonBuilderLog, Log, TEXT("Cannot build dungeon due to invalid reference"));
        return;
    }

    BuildDungeon(Dungeon->GetModel(), Dungeon->GetConfig(), Dungeon->GetQuery(), InWorld);
}

void UDungeonBuilder::BuildDungeon(UDungeonModel* InModel, UDungeonConfig* InConfig, UDungeonQuery* InQuery,
                                   UWorld* InWorld) {
    this->DungeonModel = InModel;
    this->DungeonConfig = InConfig;
    this->DungeonQuery = InQuery;

    if (DungeonQuery && DungeonQuery->UserState) {
        DungeonQuery->UserState->ClearAllState();
    }

    _SocketIdCounter = 0;
    nrandom.Init(DungeonConfig->Seed);
    Random.Initialize(DungeonConfig->Seed);

    BuildDungeonImpl(InWorld);

    // Generate and cache the dungeon layout
    if (InModel) {
        InModel->DungeonLayout = {};
        InModel->GenerateLayoutData(InConfig, InModel->DungeonLayout);
    }

    bBuildSucceeded = IdentifyBuildSucceeded();
}

void UDungeonBuilder::DestroyDungeon(UDungeonModel* InModel, UDungeonConfig* InConfig, UDungeonQuery* InQuery, ADungeon* InDungeon, UWorld* InWorld) {
    this->DungeonModel = InModel;
    this->DungeonConfig = InConfig;
    this->DungeonQuery = InQuery;
    this->Dungeon = InDungeon;

    DestroyDungeonImpl(InWorld);
    
    if (InModel) {
        InModel->DungeonLayout = {};
    }
}

void UDungeonBuilder::BuildNonThemedDungeon(ADungeon* InDungeon, TSharedPtr<FDungeonSceneProvider> InSceneProvider,
                                            UWorld* InWorld) {
    this->Dungeon = InDungeon;
    if (!Dungeon) {
        UE_LOG(DungeonBuilderLog, Log, TEXT("Cannot build dungeon due to invalid reference"));
        return;
    }

    BuildNonThemedDungeon(Dungeon->GetModel(), Dungeon->GetConfig(), Dungeon->GetQuery(), InSceneProvider, InWorld);
}

void UDungeonBuilder::BuildNonThemedDungeon(UDungeonModel* InModel, UDungeonConfig* InConfig, UDungeonQuery* InQuery,
                                            TSharedPtr<FDungeonSceneProvider> InSceneProvider, UWorld* InWorld) {
    this->DungeonModel = InModel;
    this->DungeonConfig = InConfig;
    this->DungeonQuery = InQuery;

    if (DungeonQuery && DungeonQuery->UserState) {
        DungeonQuery->UserState->ClearAllState();
    }

    _SocketIdCounter = 0;
    nrandom.Init(DungeonConfig->Seed);
    Random.Initialize(DungeonConfig->Seed);

    BuildNonThemedDungeonImpl(InWorld, InSceneProvider);

    if (InModel) {
        InModel->DungeonLayout = {};
        InModel->GenerateLayoutData(InConfig, InModel->DungeonLayout);
    }
    
    bBuildSucceeded = IdentifyBuildSucceeded();
}

void UDungeonBuilder::DestroyNonThemedDungeon(UDungeonModel* InModel, UDungeonConfig* InConfig, UDungeonQuery* InQuery,
                                              ADungeon* InDungeon, UWorld* InWorld) {
    this->DungeonModel = InModel;
    this->DungeonConfig = InConfig;
    this->DungeonQuery = InQuery;
    this->Dungeon = InDungeon;

    DestroyNonThemedDungeonImpl(InWorld);
    if (InModel) {
        InModel->DungeonLayout = {};
    }
}

void UDungeonBuilder::ApplyDungeonTheme(const TArray<UDungeonThemeAsset*>& InThemes,
                                        const TArray<FClusterThemeInfo>& InClusteredThemes,
                                        const TArray<UDungeonMarkerEmitter*>& InMarkerEmitters,
                                        TSharedPtr<FDungeonSceneProvider> InSceneProvider, UWorld* InWorld) {

    // Prepare the Theme Engine settings
    FDungeonThemeEngineSettings ThemeEngineSettings;
    ThemeEngineSettings.Themes = InThemes;
    ThemeEngineSettings.ClusteredThemes = InClusteredThemes;
    ThemeEngineSettings.SceneProvider = InSceneProvider;
    ThemeEngineSettings.MarkerEmitters = InMarkerEmitters;
    ThemeEngineSettings.DungeonBuilder = this;
    ThemeEngineSettings.DungeonModel = DungeonModel;
    ThemeEngineSettings.DungeonConfig = DungeonConfig;
    ThemeEngineSettings.DungeonQuery = DungeonQuery;

    {
        const FTransform DungeonTransform = Dungeon ? Dungeon->GetActorTransform() : FTransform::Identity;
        ThemeEngineSettings.MarkerGenerator = CreateMarkerGenProcessor(DungeonTransform);
    }
    if (Dungeon) {
        ThemeEngineSettings.bRoleAuthority = Dungeon->HasAuthority(); 
    }
    
    // Grab the theme override volumes
    if (InWorld) {
        for (TActorIterator<ADungeonThemeOverrideVolume> VolumeIt(InWorld); VolumeIt; ++VolumeIt) {
            ADungeonThemeOverrideVolume* ThemeOverrideVolume = *VolumeIt;
            if (!IsValid(ThemeOverrideVolume) || !ThemeOverrideVolume->IsValidLowLevel()) {
                continue;
            }
            const bool bValid = !Dungeon || (ThemeOverrideVolume->Dungeon == Dungeon);
            if (bValid && ThemeOverrideVolume->ThemeOverride) {
                ThemeEngineSettings.ThemeOverrideVolumes.Add(ThemeOverrideVolume);
            }
        }
    }

    // Prepare the Theme Engine callback handlers
    FDungeonThemeEngineEventHandlers EventHandlers;
    EventHandlers.PerformSelectionLogic = [this](const TArray<UDungeonSelectorLogic*>& SelectionLogics, const FDAMarkerInfo& InMarkerInfo) {
        return PerformSelectionLogic(SelectionLogics, InMarkerInfo);
    };
    
    EventHandlers.PerformTransformLogic = [this](const TArray<UDungeonTransformLogic*>& TransformLogics, const FDAMarkerInfo& InMarkerInfo) {
        return PerformTransformLogic(TransformLogics, InMarkerInfo);
    };

    EventHandlers.PerformProceduralTransformLogic = [this](const TArray<UProceduralDungeonTransformLogic*>& ProceduralTransformLogics, const FDAMarkerInfo& InMarkerInfo) {
        return PerformProceduralTransformLogic(ProceduralTransformLogics, InMarkerInfo);
    };

    EventHandlers.ProcessSpatialConstraint = [this](UDungeonSpatialConstraint* SpatialConstraint, const FTransform& Transform, FQuat& OutRotationOffset) {
        return ProcessSpatialConstraint(SpatialConstraint, Transform, OutRotationOffset);
    };

    EventHandlers.HandlePostMarkersEmit = [this](TArray<FDungeonMarkerInfo>& MarkersToEmit) {
        DungeonUtils::FDungeonEventListenerNotifier::NotifyMarkersEmitted(Dungeon, MarkersToEmit);
    };

    // Invoke the Theme Engine
    FDungeonThemeEngine::Apply(WorldMarkers, Random, ThemeEngineSettings, EventHandlers);
}

void UDungeonBuilder::MirrorDungeon() {
    if (Dungeon) {
        for (TObjectIterator<ADungeonMirrorVolume> Volume; Volume; ++Volume) {
            if (!Volume || !IsValid(*Volume) || !Volume->IsValidLowLevel()) {
                continue;
            }
            if (Volume->Dungeon == Dungeon) {
                // Build a lookup of the theme for faster access later on
                MirrorDungeonWithVolume(*Volume);
            }
        }
    }
}

TSharedPtr<FDungeonSceneProvider> UDungeonBuilder::CreateSceneProvider(UDungeonConfig* pConfig, ADungeon* InDungeon,
                                                                       UWorld* InWorld) {
    if (!pConfig) {
        UE_LOG(DungeonBuilderLog, Error, TEXT("Invalid config reference"));
        return nullptr;
    }

    TSharedPtr<FDungeonSceneProvider> SceneProvider;
    if (pConfig->Instanced) {
        SceneProvider = MakeShareable(new FInstancedDungeonSceneProvider(InDungeon, InWorld));
    }
    else {
        SceneProvider = MakeShareable(new FPooledDungeonSceneProvider(InDungeon, InWorld));
    }

    return SceneProvider;
}

bool UDungeonBuilder::ProcessSpatialConstraint(UDungeonSpatialConstraint* InSpatialConstraint,
                                               const FTransform& InTransform, FQuat& OutRotationOffset) {
    return true;
}

void UDungeonBuilder::AddMarker(const FString& InMarkerName, const FTransform& InTransform, int InCount,
                                const FVector& InterOffset, TSharedPtr<class IDungeonMarkerUserData> InUserData) {
    FTransform transform = InTransform;
    FVector Location = transform.GetLocation();
    for (int i = 0; i < InCount; i++) {
        AddMarker(InMarkerName, transform, InUserData);
        Location += InterOffset;
        transform.SetLocation(Location);
    }
}

void UDungeonBuilder::AddMarker(const FString& SocketType, const FTransform& InTransform,
                                TSharedPtr<class IDungeonMarkerUserData> InUserData) {
    FDAMarkerInfo Marker;
    Marker.Id = ++_SocketIdCounter;
    Marker.MarkerName = SocketType;
    Marker.Transform = InTransform;
    Marker.UserData = InUserData;
    WorldMarkers.Add(Marker);
}

void UDungeonBuilder::AddMarker(TArray<FDAMarkerInfo>& pPropSockets, const FString& SocketType,
                                const FTransform& transform, TSharedPtr<class IDungeonMarkerUserData> InUserData) {
    FDAMarkerInfo Marker;
    Marker.Id = ++_SocketIdCounter;
    Marker.MarkerName = SocketType;
    Marker.Transform = transform;
    Marker.UserData = InUserData;
    pPropSockets.Add(Marker);
}

void UDungeonBuilder::EmitDungeonMarkers_Implementation() {
    Random.Initialize(DungeonConfig->Seed);
}

void UDungeonBuilder::EmitMarker(const FString& SocketType, const FTransform& Transform) {
    AddMarker(SocketType, Transform);
}

void UDungeonBuilder::EmitCustomMarkers(TArray<UDungeonMarkerEmitter*> MarkerEmitters, EDungeonMarkerEmitterExecStage InExecutionStage) {
    for (UDungeonMarkerEmitter* MarkerEmitter : MarkerEmitters) {
        if (MarkerEmitter && MarkerEmitter->ExecutionStage == InExecutionStage) {
            MarkerEmitter->EmitMarkers(this, GetModel(), DungeonConfig, DungeonQuery);
        }
    }
}

void UDungeonBuilder::EmitProceduralMarkers(const TArray<UProceduralMarkerEmitter*>& InProceduralMarkerEmitters) {
    for (const UProceduralMarkerEmitter* ProcMarkerEmitter : InProceduralMarkerEmitters) {
        if (!ProcMarkerEmitter) continue;
        const bool bHandled = EmitProceduralMarkers(ProcMarkerEmitter);
        if (!bHandled) {
            // Give a chance to the proc emitter to handle it
            if (Dungeon) {
                ProcMarkerEmitter->HandleUnsupportedBuilder(Dungeon);
            }
        }
    }
}

UClass* UDungeonBuilder::DefaultBuilderClass() {
    return UGridDungeonBuilder::StaticClass();
}

void UDungeonBuilder::ProcessMarkerReplacementVolumes() {
    UWorld* World = Dungeon ? Dungeon->GetWorld() : nullptr;
    if (World) {
        for (TActorIterator<ADungeonMarkerReplaceVolume> VolumeIt(World); VolumeIt; ++VolumeIt) {
            ADungeonMarkerReplaceVolume* Volume = *VolumeIt;
            if (Volume && Volume->Dungeon == Dungeon) {
                ProcessMarkerReplacementVolume(Volume);
            }
        }
    }
}

void UDungeonBuilder::ProcessMarkerReplacementVolume(class ADungeonMarkerReplaceVolume* MarkerReplaceVolume) {
    if (!MarkerReplaceVolume) return;
    for (FDAMarkerInfo& Socket : WorldMarkers) {
        if (MarkerReplaceVolume->EncompassesPoint(Socket.Transform.GetLocation())) {
            for (const FMarkerReplaceEntry& Entry : MarkerReplaceVolume->Replacements) {
                if (Socket.MarkerName == Entry.MarkerName) {
                    Socket.MarkerName = Entry.ReplacementName;
                }
            }
        }
    }
}

UDungeonLevelStreamingModel* UDungeonBuilder::GetLevelStreamingModel() {
    if (!Dungeon) {
        Dungeon = Cast<ADungeon>(GetOuter());
    }
    return Dungeon ? Dungeon->LevelStreamingModel : nullptr; 
}

FTransform UDungeonBuilder::PerformProceduralTransformLogic(const TArray<UProceduralDungeonTransformLogic*>& ProceduralTransformLogics, const FDAMarkerInfo& InMarkerInfo) {
    FTransform Result{FTransform::Identity};

    for (UProceduralDungeonTransformLogic* Logic : ProceduralTransformLogics) {
        if (Logic) {
            Result = Logic->Execute(Random, InMarkerInfo.Transform) * Result;
        }
    }
    
    return Result;
}

void UDungeonBuilder::ProcessThemeItemUserData(TSharedPtr<IDungeonMarkerUserData> UserData, AActor* SpawnedActor) {
}

void UDungeonBuilder::ClearMarkerList() {
    WorldMarkers.Reset();

    if (const ADungeon* OwningDungeon = Cast<ADungeon>(GetOuter())) {
        if (UDASceneDebugDataComponent* SceneDebugData = OwningDungeon->GetComponentByClass<UDASceneDebugDataComponent>()) {
            SceneDebugData->ClearDebugData();
        }
    }
}

void UDungeonBuilder::SetLayoutOnlyPreviewBuild(bool bValue) {
    bLayoutPreviewBuildOnly = bValue;
}

void UDungeonBuilder::GetRandomStream(FRandomStream& OutRandomStream) {
    OutRandomStream = Random;
}


