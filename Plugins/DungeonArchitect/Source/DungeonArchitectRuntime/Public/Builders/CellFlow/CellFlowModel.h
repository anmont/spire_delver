//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/DungeonModel.h"
#include "CellFlowModel.generated.h"

class UCellFlowVoronoiGraph;
class UDAFlowCellGraph;
class UCellFlowLayoutGraph;

UCLASS()
class DUNGEONARCHITECTRUNTIME_API UCellFlowModel : public UDungeonModel {
	GENERATED_BODY()
public:
	virtual void Reset() override;
	virtual FDungeonFloorSettings CreateFloorSettings(const UDungeonConfig* InConfig) const override;
	virtual void GenerateLayoutData(const UDungeonConfig* InConfig, FDungeonLayoutData& OutLayout) const override;

public:	
	UPROPERTY()
	TObjectPtr<UCellFlowLayoutGraph> LayoutGraph{};

	UPROPERTY()
	TObjectPtr<UDAFlowCellGraph> CellGraph{};

	UPROPERTY()
	TObjectPtr<UCellFlowVoronoiGraph> VoronoiData{};
};

