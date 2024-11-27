//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowLibGrid.h"

#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/Tasks/CellFlowLayoutTaskScatterProps.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowLib.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowStructs.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowUtils.h"
#include "Frameworks/MarkerGenerator/MarkerGenLayer.h"
#include "Frameworks/MarkerGenerator/MarkerGenPattern.h"

void FCellFlowLibGrid::InsertEdgeMarkers(FCellFlowGridMarkerContext& Context, const FCellFlowGridEdgeInfo& Edge,
		const FDAFlowCellGraphGridInfo& GridInfo, FuncEmitGridCellMarker& EmitCellMarker) {
	
	const TArray<FCellFlowGridEdgeInfo>& HalfEdges = GridInfo.HalfEdges;
	const FCellFlowGridEdgeInfo& EdgeTwin = HalfEdges[Edge.TwinIndex];
	const FIntPoint TileCoord { Edge.Coord };
	const FIntPoint TileCoordTwin { EdgeTwin.Coord };

	FCellFlowGridMarkerSetup ChunkMarkers{};
	if (const FCellFlowGridMarkerSetup* GroupMarkerSetupPtr = Context.GroupNodeChunkMarkers.Find(Edge.TileGroup)) {
		ChunkMarkers = *GroupMarkerSetupPtr;
	}
	
	{
		FIntPoint EdgeCoordSrc, EdgeCoordDst;
		FCellFlowUtils::GetEdgeEndPoints(Edge, EdgeTwin, EdgeCoordSrc, EdgeCoordDst);

		int HeightZ = Edge.HeightZ;
		if (Edge.bContainsStair) {
			HeightZ = FMath::Max(Edge.HeightZ, EdgeTwin.HeightZ);
		}
		
		const double AngleRad = FMathUtils::FindAngle(FVector2d(EdgeCoordDst.X - EdgeCoordSrc.X, EdgeCoordDst.Y - EdgeCoordSrc.Y));
		const FVector EdgeWorldLocation = FVector(
			(TileCoord.X + TileCoordTwin.X) * 0.5 + 0.5,
			(TileCoord.Y + TileCoordTwin.Y) * 0.5 + 0.5,
			HeightZ
		) * Context.GridSize;

		const FQuat EdgeWorldRotation(FVector::UpVector, AngleRad);
		const FString MarkerName = Edge.bConnection ? ChunkMarkers.DoorMarker : ChunkMarkers.EdgeMarker;
		
		bool bEmitMarker{true};
		if (Edge.bConnection) {
			if (Edge.HeightZ < EdgeTwin.HeightZ) {
				// Use the higher door instead
				bEmitMarker = false;
			}
			else if (Edge.HeightZ == EdgeTwin.HeightZ) {
				// Choose any one of the doors
				bEmitMarker = Edge.TileGroup < EdgeTwin.TileGroup;
			}
		}

		if (bEmitMarker) {
			EmitCellMarker(MarkerName, FTransform(EdgeWorldRotation, EdgeWorldLocation));
		}

		auto InsertWallSeparator = [&](const FIntVector& InCoord) {
			if (!Context.InsertedPillarCoords.Contains(InCoord)) {
				Context.InsertedPillarCoords.Add(InCoord);
				const FVector SeparatorWorldLocation = FVector(InCoord) * Context.GridSize;
				const FTransform SeparatorTransform(EdgeWorldRotation, SeparatorWorldLocation);
				EmitCellMarker(ChunkMarkers.EdgeSeparatorMarker, SeparatorTransform);
			}
		};
		
		// Insert the Wall Separators
		InsertWallSeparator({ EdgeCoordSrc.X, EdgeCoordSrc.Y, HeightZ });
		InsertWallSeparator({ EdgeCoordDst.X, EdgeCoordDst.Y, HeightZ });
	}

	// TODO: Handle cliff depth
	if (Edge.bContainsStair) {
		if (const FDAFlowCellGraphGridStairInfo* StairInfo = GridInfo.Stairs.Find(Edge.EdgeIndex)) {
			const FQuat StairRotation({0, 0, 1}, StairInfo->AngleRadians - PI * 0.5);	// TODO: Fix the 90 degree hack
			const FVector BaseLoc = FVector(StairInfo->LocalLocation) * Context.GridSize;
			EmitCellMarker(ChunkMarkers.StairMarker, FTransform(StairRotation, BaseLoc));
		}
	}
}

void FCellFlowLibGrid::TransformPatternLayer(UMarkerGenLayer* Layer, const FCellFlowLayoutTaskScatterPropSettings& Settings) {
	if (!Layer || !Layer->Pattern) {
		return;
	}

	static const FName TagEdgeList = TEXT("EdgeList");
	static const FName TagDoorList = TEXT("DoorList");
	static const FName TagStairList = TEXT("StairList");
	static const FName TagAvoidList = TEXT("AvoidList");
	static const FName TagGroundList = TEXT("GroundList");
		
	static const FName TagProp1x1 = TEXT("Prop1x1Name");
	static const FName TagProp1x2 = TEXT("Prop1x2Name");
	static const FName TagProp1x3 = TEXT("Prop1x3Name");
	static const FName TagProp2x2 = TEXT("Prop2x2Name");
	static const FName TagProp2x3 = TEXT("Prop2x3Name");
	static const FName TagProp3x3 = TEXT("Prop3x3Name");
		
	for (UMarkerGenPatternRule* Rule : Layer->Pattern->Rules) {
		if (!Rule->RuleScript) continue;
		for (UMGPatternScriptNode* Node : Rule->RuleScript->Nodes) {
			if (UMGPatternScriptNode_MarkerListExists* MarkerListNode = Cast<UMGPatternScriptNode_MarkerListExists>(Node)) {
				MarkerListNode->MarkerNames.Reset();
				if (MarkerListNode->Tags.Contains(TagEdgeList)) {
					MarkerListNode->MarkerNames.Append(Settings.EdgeMarkerNames);
				}
				if (MarkerListNode->Tags.Contains(TagGroundList)) {
					MarkerListNode->MarkerNames.Append(Settings.GroundMarkerNames);
				}
				if (MarkerListNode->Tags.Contains(TagDoorList)) {
					MarkerListNode->MarkerNames.Append(Settings.DoorMarkerNames);
				}
				if (MarkerListNode->Tags.Contains(TagStairList)) {
					MarkerListNode->MarkerNames.Append(Settings.StairMarkerNames);
				}
				if (MarkerListNode->Tags.Contains(TagAvoidList)) {
					MarkerListNode->MarkerNames.Append(Settings.TilesMarkersToAvoid);
				}
			}
			else {
				FString *MarkerName{};
				if (UMGPatternScriptNode_EmitMarker* EmitNode = Cast<UMGPatternScriptNode_EmitMarker>(Node)) {
					MarkerName = &EmitNode->MarkerName;
				}
				else if (UMGPatternScriptNode_RemoveMarker* RemoveNode = Cast<UMGPatternScriptNode_RemoveMarker>(Node)) {
					MarkerName = &RemoveNode->MarkerName;
				}

				if (MarkerName) {
					if (Node->Tags.Contains(TagProp1x1)) {
						*MarkerName = Settings.Prop1x1.MarkerName;
					}
					if (Node->Tags.Contains(TagProp1x2)) {
						*MarkerName = Settings.Prop1x2.MarkerName;
					}
					if (Node->Tags.Contains(TagProp1x3)) {
						*MarkerName = Settings.Prop1x3.MarkerName;
					}
				}
			}
		}
	}
}

