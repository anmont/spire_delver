//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Builders/Grid/GridDungeonModel.h"
#include "Frameworks/ThemeEngine/Rules/Selector/DungeonSelectorLogic.h"
#include "GridDungeonSelectorLogic.generated.h"

/**
*
*/
UCLASS(Blueprintable, HideDropDown)
class DUNGEONARCHITECTRUNTIME_API UGridDungeonSelectorLogic : public UDungeonSelectorLogic {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = Dungeon)
    bool IsOnCorner(UGridDungeonModel* Model, int32 GridX, int32 GridY);

    UFUNCTION(BlueprintCallable, Category = Dungeon)
    bool IsPillarOnCorner(UGridDungeonModel* Model, int32 GridX, int32 GridY, FTransform& OutCornerOffset);

    UFUNCTION(BlueprintCallable, Category = Dungeon)
    bool IsPassageTooNarrow(UGridDungeonModel* Model, int32 GridX, int32 GridY);

    UFUNCTION(BlueprintCallable, Category = Dungeon)
    bool ContainsStair(UGridDungeonModel* Model, const FGridDungeonCell& Cell, int32 GridX, int32 GridY);

    /** called when something enters the sphere component */
    UFUNCTION(BlueprintNativeEvent, Category = "Dungeon")
    bool SelectNode(UGridDungeonModel* Model, UGridDungeonConfig* Config, UGridDungeonBuilder* Builder,
                    UGridDungeonQuery* Query, const FGridDungeonCell& Cell, const FRandomStream& RandomStream, int32 GridX,
                    int32 GridY, const FTransform& MarkerTransform);
    virtual bool SelectNode_Implementation(UGridDungeonModel* Model, UGridDungeonConfig* Config,
                                           UGridDungeonBuilder* Builder, UGridDungeonQuery* Query, const FGridDungeonCell& Cell,
                                           const FRandomStream& RandomStream, int32 GridX, int32 GridY,
                                           const FTransform& MarkerTransform);

private:
    FGridDungeonCell* GetCell(UGridDungeonModel* Model, int32 GridX, int32 GridY);
    bool IsDifferentCell(FGridDungeonCell* Cell0, FGridDungeonCell* Cell1);

private:


};

