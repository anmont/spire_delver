//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowStructs.h"

struct FDAFlowCellGraphGridInfo;
class UMarkerGenLayer;
struct FCellFlowLayoutTaskScatterPropSettings;
struct FCellFlowGridEdgeInfo;

class FCellFlowLibGrid {
public:
	struct FCellFlowGridMarkerContext {
		FVector GridSize;
		TSet<FIntVector> InsertedPillarCoords;
		TMap<int32, FCellFlowGridMarkerSetup> GroupNodeChunkMarkers;
	};
	
	typedef TFunction<void(const FString& /* MarkerName */, const FTransform& /* MarkerTransform */)> FuncEmitGridCellMarker;
	static void InsertEdgeMarkers(FCellFlowGridMarkerContext& Context, const FCellFlowGridEdgeInfo& Edge, const FDAFlowCellGraphGridInfo& GridInfo, FuncEmitGridCellMarker& EmitCellMarker);
	static void TransformPatternLayer(UMarkerGenLayer* Layer, const FCellFlowLayoutTaskScatterPropSettings& Settings);
};

