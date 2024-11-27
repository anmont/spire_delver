//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Layout/DungeonLayoutData.h"
#include "Core/Layout/FloorSettings.h"
#include "DungeonModel.generated.h"

class UDungeonConfig;
class UDungeonSpawnLogic;
struct FDungeonLayoutData;

class DUNGEONARCHITECTRUNTIME_API IDungeonMarkerUserData {
};

USTRUCT(Blueprintable)
struct DUNGEONARCHITECTRUNTIME_API FDungeonMarkerInfo {
    GENERATED_USTRUCT_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Dungeon)
    FTransform transform;

    UPROPERTY()
    FName NodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Dungeon)
    TArray<UDungeonSpawnLogic*> SpawnLogics;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Dungeon)
    UObject* TemplateObject = nullptr;

    TSharedPtr<IDungeonMarkerUserData> UserData;

    FORCEINLINE bool operator==(const FDungeonMarkerInfo& other) const {
        return other.NodeId == NodeId;
    }
};

UCLASS(Blueprintable)
class DUNGEONARCHITECTRUNTIME_API UDungeonModel : public UObject {
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category="Dungeon")
    FDungeonLayoutData DungeonLayout;
    
public:
    virtual void Reset() {}
    virtual void PostBuildCleanup() {}
    virtual bool ShouldAutoResetOnBuild() const { return true; }
    virtual void GenerateLayoutData(const UDungeonConfig* InConfig, struct FDungeonLayoutData& OutLayout) const {}
    virtual FDungeonFloorSettings CreateFloorSettings(const UDungeonConfig* InConfig) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Dungeon")
    FBox GetDungeonBounds() const;
};

