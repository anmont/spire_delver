//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowLib.h"
#include "CellFlowGrid.generated.h"

UCLASS()
class DUNGEONARCHITECTRUNTIME_API UDAFlowCellLeafNodeGrid : public UDAFlowCellLeafNode {
	GENERATED_BODY()
public:
	UPROPERTY()
	FIntPoint Location;
	
	UPROPERTY()
	FIntPoint Size;

	virtual FVector2d GetCenter() const override;
	virtual float GetArea() const override;
};

