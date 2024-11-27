//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/DungeonProp.h"

class UDungeonConfig;
class UMarkerGenLayer;
class UMarkerGenPattern;

UENUM()
enum class EMGMarkerListDuplicateCheckMethod {
	None,
	CheckLocationOnly,
	CheckLocationAndRotation,
	CheckFullTransform
};

struct FMGExecEmitMarkerSettings {
	TArray<FString> CopyRotationFromMarkers;
	TArray<FString> CopyHeightFromMarkers;
	EMGMarkerListDuplicateCheckMethod DuplicateMarkerRemoveRule { EMGMarkerListDuplicateCheckMethod::CheckLocationOnly };
};

class DUNGEONARCHITECTRUNTIME_API IMGPatternRuleExecutor {
public:
	virtual ~IMGPatternRuleExecutor() {}
	
	// Condition Functions
	virtual bool ContainsMarker(const FString& InMarkerName) const = 0;
	
	// Action Functions
	virtual void EmitMarker(const FString& InMarkerName, const FMGExecEmitMarkerSettings& InSettings) = 0;
	virtual void RemoveMarker(const FString& InMarkerName) = 0;
};

struct DUNGEONARCHITECTRUNTIME_API FMGActionExecContext {
	TSharedPtr<IMGPatternRuleExecutor> Executor;
	TMap<FString, FString> Variables;
};

struct DUNGEONARCHITECTRUNTIME_API FMGConditionExecContext {
	TSharedPtr<IMGPatternRuleExecutor> Executor;
	TMap<FString, FString> Variables;
}; 

class IMarkerGenProcessor {
public:
	IMarkerGenProcessor(const FTransform& InDungeonTransform) : DungeonTransform(InDungeonTransform) {}
	virtual ~IMarkerGenProcessor() {}
	virtual bool Process(const UMarkerGenLayer* InLayer, const TArray<FDAMarkerInfo>& InMarkers, const FRandomStream& InRandom, TArray<FDAMarkerInfo>& OutMarkers) = 0;

protected:
	FTransform DungeonTransform = FTransform::Identity;
};


class FMarkerGenProcessorFactory {
public:
	static TSharedPtr<IMarkerGenProcessor> Create_DEPRECATED(const UDungeonConfig* InConfig, const FTransform& InDungeonTransform);
};

