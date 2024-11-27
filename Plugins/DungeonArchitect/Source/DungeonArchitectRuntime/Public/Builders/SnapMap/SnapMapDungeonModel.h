//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Frameworks/Snap/Lib/SnapDungeonModelBase.h"
#include "SnapMapDungeonModel.generated.h"

struct FSnapConnectionInstance;
class UGrammarScriptGraph;
class USnapMapDungeonLevelLoadHandler;

UCLASS(Blueprintable)
class DUNGEONARCHITECTRUNTIME_API USnapMapDungeonModel : public USnapDungeonModelBase {
    GENERATED_UCLASS_BODY()

public:
    virtual void Reset() override;
    virtual bool ShouldAutoResetOnBuild() const override { return false; }
    bool SearchModuleInstance(const FGuid& InNodeId, FSnapModuleInstanceSerializedData& OutModuleData);

    virtual FDungeonFloorSettings CreateFloorSettings(const UDungeonConfig* InConfig) const override;
    
public:
    UPROPERTY()
    UGrammarScriptGraph* MissionGraph;
};

