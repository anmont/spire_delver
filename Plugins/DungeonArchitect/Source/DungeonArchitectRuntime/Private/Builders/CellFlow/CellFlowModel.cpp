//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Builders/CellFlow/CellFlowModel.h"

#include "Builders/CellFlow/CellFlowConfig.h"
#include "Core/Layout/DungeonLayoutData.h"
#include "Frameworks/Flow/Domains/LayoutGraph/Core/FlowAbstractNode.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/CellFlowLayoutGraph.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/Impl/CellFlowGrid.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/Impl/CellFlowVoronoi.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/Tasks/CellFlowLayoutTaskCreateCellsVoronoi.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowLib.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowUtils.h"

void UCellFlowModel::Reset() {
	LayoutGraph = {};
	CellGraph = {};
	VoronoiData = {};
}

FDungeonFloorSettings UCellFlowModel::CreateFloorSettings(const UDungeonConfig* InConfig) const {
	FDungeonFloorSettings Settings = {};
	if (const UCellFlowConfig* CellConfig = Cast<UCellFlowConfig>(InConfig)) {
		Settings.FloorHeight = CellConfig->GridSize.Z;
		Settings.FloorCaptureHeight = Settings.FloorHeight - 1;

		if (CellGraph && CellGraph->LeafNodes.Num() > 0) {
			int32 MinZ{};
			int32 MaxZ{};
			MinZ = MaxZ = CellGraph->LeafNodes[0]->LogicalZ;

			for (UDAFlowCellLeafNode* LeafNode : CellGraph->LeafNodes) {
				MinZ = FMath::Min(MinZ, LeafNode->LogicalZ);
				MaxZ = FMath::Max(MaxZ, LeafNode->LogicalZ);
			}

			Settings.FloorLowestIndex = MinZ;
			Settings.FloorHighestIndex = MaxZ;
		}
	}
	return Settings;
}

void UCellFlowModel::GenerateLayoutData(const UDungeonConfig* InConfig, FDungeonLayoutData& OutLayout) const {
	if (!LayoutGraph || !CellGraph) {
		return;
	}
	
	const UCellFlowConfig* CellFlowConfig = Cast<const UCellFlowConfig>(InConfig);
	const AActor* DungeonActor = Cast<AActor>(GetOuter());
	if (!CellFlowConfig || !DungeonActor) {
		return;
	}

	const FTransform DungeonTransform = DungeonActor->GetActorTransform();
	const FVector& GridSize = CellFlowConfig->GridSize;
	
	TMap<FGuid, const UFlowAbstractNode*> LayoutNodes;
	for (const UFlowAbstractNode* GraphNode : LayoutGraph->GraphNodes) {
		const UFlowAbstractNode*& NodeRef = LayoutNodes.FindOrAdd(GraphNode->NodeId);
		NodeRef = GraphNode;
	}

	
	auto InsertTileShape = [&](FDungeonLayoutDataChunkInfo& OutChunkShape, const FVector& Location, const FVector& Size) {
		const FTransform Transform = FTransform(Location + Size * 0.5f) * DungeonTransform;

		FVector WorldLocation = Transform.TransformPosition(Location);
		FDABoundsShapeConvexPoly& OutCellPoly = OutChunkShape.ConvexPolys.AddDefaulted_GetRef();
		OutCellPoly.Height = WorldLocation.Z;
		OutCellPoly.Transform = FTransform::Identity;
		FDABoundsShapeCollision::ConvertBoxToConvexPoly(Transform, Size * 0.5f, OutCellPoly);
		
		FBox CellBox(Location, Location + Size);
		CellBox = CellBox.TransformBy(DungeonTransform);
		OutLayout.Bounds += CellBox;
	};

	
	// Emit the cell polygons
	for (const FDAFlowCellGroupNode& GroupNode : CellGraph->GroupNodes) {
		if (!GroupNode.IsActive()) continue;
		if (GroupNode.LeafNodes.Num() == 0) continue;

		const UFlowAbstractNode** LayoutNodePtr = LayoutNodes.Find(GroupNode.LayoutNodeID);
		const UFlowAbstractNode* LayoutNode = LayoutNodePtr ? *LayoutNodePtr : nullptr;
		if (!LayoutNode || !LayoutNode->bActive) {
			continue;
		}

		const TArray<FVector2d>& Sites = VoronoiData->Sites;
		const DA::DCELGraph& DGraph = VoronoiData->DGraph;
		const TArray<DA::DCEL::FFace*>& Faces = DGraph.GetFaces();
		
		FDungeonLayoutDataChunkInfo& OutChunkShape = OutLayout.ChunkShapes.AddDefaulted_GetRef();
		
		// Emit grid cell quads 
		for (const int LeafNodeId : GroupNode.LeafNodes) {
			if (const UDAFlowCellLeafNodeGrid* GridLeafNode = Cast<UDAFlowCellLeafNodeGrid>(CellGraph->LeafNodes[LeafNodeId])) {
				const FVector NodeSize(GridLeafNode->Size.X, GridLeafNode->Size.Y, 0);
				FVector Location = FVector(GridLeafNode->Location.X, GridLeafNode->Location.Y, GridLeafNode->LogicalZ) * GridSize;
				FVector Size = NodeSize * FVector(GridSize.X, GridSize.Y, 0);

				InsertTileShape(OutChunkShape, Location, Size);
			}
		}
		
		// Emit the voronoi leaf quad
		for (const int LeafNodeId : GroupNode.LeafNodes) {
			if (Faces.IsValidIndex(LeafNodeId)) {
				if (UDAFlowCellLeafNodeVoronoi* LeafNode = Cast<UDAFlowCellLeafNodeVoronoi>(CellGraph->LeafNodes[LeafNodeId])) {
					const DA::DCEL::FFace* Face = Faces[LeafNodeId];
					if (!Face || !Face->bValid || !Face->Outer) continue;
					if (!Sites.IsValidIndex(LeafNodeId)) {
						continue;
					}

					TArray<const DA::DCEL::FEdge*> FaceEdges;
					DA::DCEL::TraverseFaceEdges(Face->Outer, [&](const DA::DCEL::FEdge* InEdge) {
						FaceEdges.Add(InEdge);
					});

					FDABoundsShapeConvexPoly& SitePoly = OutChunkShape.ConvexPolys.AddDefaulted_GetRef();
					SitePoly.Height = LeafNode->LogicalZ * GridSize.Z;
					SitePoly.Transform = DungeonTransform;
					for (const DA::DCEL::FEdge* FaceEdge : FaceEdges) {
						if (FaceEdge && FaceEdge->Origin) {
							FVector2D EdgeStart = FVector2D(FaceEdge->Origin->Location) * FVector2D(GridSize);
							SitePoly.Points.Add(EdgeStart);
							OutLayout.Bounds += DungeonTransform.TransformPosition(FVector(EdgeStart, SitePoly.Height));
						}
					}
				}
			}
		}
	}

	// Emit the stairs
	{
		// Emit out the grid stair info
		for (const auto& StairEntry : CellGraph->GridInfo.Stairs) {
			int32 EdgeIndex = StairEntry.Key;
			const FDAFlowCellGraphGridStairInfo& StairInfo = StairEntry.Value;
			FQuat StairRotation({0, 0, 1}, StairInfo.AngleRadians);
			FVector StairLocation = StairInfo.LocalLocation * GridSize;
			
			FDungeonLayoutDataStairItem& OutStair = OutLayout.Stairs.AddDefaulted_GetRef();
			OutStair.WorldTransform = FTransform(FRotator(0, -90, 0)) * FTransform(StairRotation, StairLocation) * DungeonTransform;
			OutStair.Width = GridSize.X;
		}

		
		// Emit out the voronoi stair info
		for (const auto& StairEntry : CellGraph->DCELInfo.Stairs) {
			const FDAFlowCellGraphDCELEdgeInfo& StairInfo = StairEntry.Value;
			
			const FVector Scale = FVector(StairInfo.LogicalWidth, 1, 1);
			const FQuat Rotation = FQuat::FindBetweenVectors({0, -1, 0}, StairInfo.Direction);
			const FVector BaseLocation = StairInfo.LogicalLocation;
			const FVector Location = (BaseLocation + StairInfo.Direction * 0.5f) * GridSize;
			
			FDungeonLayoutDataStairItem& OutStair = OutLayout.Stairs.AddDefaulted_GetRef();
			OutStair.WorldTransform = FTransform(FRotator(0, -90, 0)) * FTransform(Rotation, Location, Scale) * DungeonTransform;
			OutStair.Width = StairInfo.LogicalWidth * GridSize.X;
		}
	}

	// Emit the door info
	{
		// Emit the grid doors
		for (const FCellFlowGridEdgeInfo& Edge : CellGraph->GridInfo.HalfEdges) {
			if (!Edge.bConnection) {
				continue;
			}
			if (!CellGraph->GridInfo.HalfEdges.IsValidIndex(Edge.TwinIndex)) {
				continue;
			}
			
			FCellFlowGridEdgeInfo& EdgeTwin = CellGraph->GridInfo.HalfEdges[Edge.TwinIndex];
			if (EdgeTwin.bConnection) {
				if (EdgeTwin.HeightZ > Edge.HeightZ) {
					// the twin is at a higher height.  We'll process that instead when we get to it
					continue;
				}
				else if (EdgeTwin.HeightZ == Edge.HeightZ) {
					if (EdgeTwin.TileGroup < Edge.TileGroup) {
						// The two sides of the door are on the same height. we'll process this only once
						continue;
					}
				}
			}
			
			FDungeonLayoutDataDoorItem&	OutDoor = OutLayout.Doors.AddDefaulted_GetRef();
			OutDoor.Width = GridSize.X;
			OutDoor.DoorOcclusionThickness = GridSize.X;
			
			const FIntPoint TileCoord { Edge.Coord };
			const FIntPoint TileCoordTwin { EdgeTwin.Coord };
			FIntPoint EdgeCoordSrc, EdgeCoordDst;
			FCellFlowUtils::GetEdgeEndPoints(Edge, EdgeTwin, EdgeCoordSrc, EdgeCoordDst);
			
			const double AngleRad = FMathUtils::FindAngle(FVector2d(EdgeCoordDst.X - EdgeCoordSrc.X, EdgeCoordDst.Y - EdgeCoordSrc.Y));
			const FVector EdgeWorldLocation = FVector(
				(TileCoord.X + TileCoordTwin.X) * 0.5f + 0.5f,
				(TileCoord.Y + TileCoordTwin.Y) * 0.5f + 0.5f,
				Edge.HeightZ
			) * GridSize;

			const FQuat EdgeWorldRotation(FVector::UpVector, AngleRad);
			OutDoor.WorldTransform = FTransform(FRotator(0, -90, 0)) * FTransform(EdgeWorldRotation, EdgeWorldLocation) * DungeonTransform;
		}

		// Emit out the voronoi door info
		for (const auto& DoorEntry : CellGraph->DCELInfo.Doors) {
			const FDAFlowCellGraphDCELEdgeInfo& DoorInfo = DoorEntry.Value;
			
			const FVector Scale = FVector(DoorInfo.LogicalWidth, 1, 1);
			const FQuat Rotation = FQuat::FindBetweenVectors({0, -1, 0}, DoorInfo.Direction);
			const FVector Location = DoorInfo.LogicalLocation * GridSize;
			
			FDungeonLayoutDataDoorItem& OutDoor = OutLayout.Doors.AddDefaulted_GetRef();
			OutDoor.WorldTransform = FTransform(FRotator(0, -90, 0)) * FTransform(Rotation, Location, Scale) * DungeonTransform;
			OutDoor.Width = DoorInfo.LogicalWidth * GridSize.X;
			OutDoor.DoorOcclusionThickness = GridSize.X * 0.5f;
		}
	}
}

