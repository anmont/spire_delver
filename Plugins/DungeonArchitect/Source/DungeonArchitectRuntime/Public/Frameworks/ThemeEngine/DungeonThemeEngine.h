//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Frameworks/MarkerGenerator/MarkerGenProcessor.h"
#include "Frameworks/ThemeEngine/DungeonThemeAsset.h"

class FDungeonSceneProvider;
struct FDungeonMarkerInfo;
struct FDAMarkerInfo;
class ADungeonThemeOverrideVolume;
class UDungeonSelectorLogic;
class UDungeonTransformLogic;
class UDungeonSpatialConstraint;

struct FDungeonThemeEngineSettings {
    TArray<UDungeonThemeAsset*> Themes;
    TArray<FClusterThemeInfo> ClusteredThemes;
    TArray<ADungeonThemeOverrideVolume*> ThemeOverrideVolumes;
    TSharedPtr<FDungeonSceneProvider> SceneProvider;
	TSharedPtr<IMarkerGenProcessor> MarkerGenerator;
    TArray<UDungeonMarkerEmitter*> MarkerEmitters;
	TObjectPtr<UDungeonBuilder> DungeonBuilder;
	TObjectPtr<UDungeonConfig> DungeonConfig;
	TObjectPtr<UDungeonModel> DungeonModel;
	TObjectPtr<UDungeonQuery> DungeonQuery;
	bool bRoleAuthority = true;
};

struct FDungeonThemeEngineEventHandlers {
    TFunction<bool(const TArray<UDungeonSelectorLogic*>&, const FDAMarkerInfo&)> PerformSelectionLogic
            = [](const TArray<UDungeonSelectorLogic*>&, const FDAMarkerInfo&){ return false; };

	TFunction<FTransform(const TArray<UDungeonTransformLogic*>&, const FDAMarkerInfo&)> PerformTransformLogic
			= [](const TArray<UDungeonTransformLogic*>&, const FDAMarkerInfo&) { return FTransform::Identity; };

	TFunction<FTransform(const TArray<UProceduralDungeonTransformLogic*>&, const FDAMarkerInfo&)> PerformProceduralTransformLogic
		= [](const TArray<UProceduralDungeonTransformLogic*>&, const FDAMarkerInfo&) { return FTransform::Identity; };

	TFunction<bool(UDungeonSpatialConstraint*, const FTransform&, FQuat&)> ProcessSpatialConstraint
            = [](UDungeonSpatialConstraint*, const FTransform&, FQuat&) { return true; };

    TFunction<void(TArray<FDungeonMarkerInfo>&)> HandlePostMarkersEmit
            = [](TArray<FDungeonMarkerInfo>&) {};
};

class DUNGEONARCHITECTRUNTIME_API FDungeonThemeEngine {
public:
    static void Apply(TArray<FDAMarkerInfo>& Markers, const FRandomStream& InRandom,
                const FDungeonThemeEngineSettings& InSettings, const FDungeonThemeEngineEventHandlers& EventHandlers);

};


class DUNGEONARCHITECTRUNTIME_API FDungeonThemeEngineUtils {
public:
    FORCEINLINE static FName CreateNodeTagFromId(const FName& NodeId) {
	    return *FString("NODE-").Append(NodeId.ToString());
    }    
};        

