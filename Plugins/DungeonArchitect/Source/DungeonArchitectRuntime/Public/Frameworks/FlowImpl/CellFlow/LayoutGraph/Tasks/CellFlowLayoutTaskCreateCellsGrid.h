//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/Tasks/CellFlowLayoutTaskCreateCellsBase.h"
#include "CellFlowLayoutTaskCreateCellsGrid.generated.h"

UCLASS(Meta = (AbstractTask, Title="Create Cells (Grid)", Tooltip="Create the initial grid cell graph to work on", MenuPriority = 1000))
class UCellFlowLayoutTaskCreateCellsGrid : public UCellFlowLayoutTaskCreateCellsBase {
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "CellFlow")
	int MinCellSize = 1;
	
	UPROPERTY(EditAnywhere, Category = "CellFlow")
	int MaxCellSize = 6;
	
	UPROPERTY(EditAnywhere, Category = "CellFlow")
	int FitIterations = 20;
	
	UPROPERTY(EditAnywhere, Category = "CellFlow")
	float MinAspectRatio = 0.4f;
	
	UPROPERTY(EditAnywhere, Category = "CellFlow")
	float MaxAspectRatio = 1.5f;
	
private:
	virtual void GenerateCellsImpl(UDAFlowCellGraph* InCellGraph, const FRandomStream& InRandom, FFlowExecNodeStatePtr OutputState) const override;
};

