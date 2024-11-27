//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowLib.h"

#include "Builders/CellFlow/CellFlowConfig.h"
#include "Frameworks/Flow/Domains/LayoutGraph/Tasks/Lib/FlowAbstractGraphPathUtils.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/Impl/CellFlowGrid.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowStats.h"

const FName UDAFlowCellGraph::StateTypeID = TEXT("CellGraphStateObject");

void UDAFlowCellGraph::CloneFromStateObject(const UObject* SourceObject) {
	const UDAFlowCellGraph* OtherGraph = Cast<UDAFlowCellGraph>(SourceObject);
	if (!OtherGraph) return;

	// Clone the group nodes. Since they are plain data objects, just copy them over
	GroupNodes = OtherGraph->GroupNodes;

	// Clone the leaf nodes
	LeafNodes.Reset();
	for (UDAFlowCellLeafNode* OtherLeafNode : OtherGraph->LeafNodes) {
		UDAFlowCellLeafNode* ClonedLeafNode = NewObject<UDAFlowCellLeafNode>(this, OtherLeafNode->GetClass(), NAME_None, RF_NoFlags, OtherLeafNode);
		LeafNodes.Add(ClonedLeafNode);
	}

	ScatterSettings = OtherGraph->ScatterSettings;
	
	GridInfo = OtherGraph->GridInfo;
	DCELInfo = OtherGraph->DCELInfo;
	SceneDebugData = OtherGraph->SceneDebugData;
	
#if WITH_EDITORONLY_DATA
	bRenderInactiveGroups = OtherGraph->bRenderInactiveGroups;
#endif // WITH_EDITORONLY_DATA
}

namespace DA {
	void FCellGraphBuilder::CollapseEdges(UDAFlowCellGraph* InGraph, int MinGroupArea, const FRandomStream& Random, FCellAreaLookup& AreaLookup) {
		SCOPE_CYCLE_COUNTER(STAT_CFBuild_CollapseEdges);
		while (CollapsedBestGroupEdge(InGraph, MinGroupArea, Random, AreaLookup)) {}
	}
	
	void FCellGraphBuilder::AssignGroupColors(UDAFlowCellGraph* InGraph) {
		TArray<int> ActiveGroupIds;
		for (const FDAFlowCellGroupNode &GroupNode: InGraph->GroupNodes) {
			if (GroupNode.IsActive()) {
				ActiveGroupIds.Add(GroupNode.GroupId);
			}
		}

		const int NumActiveGroups = ActiveGroupIds.Num();
		if (NumActiveGroups > 0) {
			const float AngleDelta = 360.0f / static_cast<float>(NumActiveGroups);
			for (int i = 0; i < NumActiveGroups; i++) {
				const float Hue = AngleDelta * i;
				const int GroupId = ActiveGroupIds[i];
				FLinearColor HSV(Hue, 0.3f, 1.0f);
					
				InGraph->GroupNodes[GroupId].GroupColor = HSV.HSVToLinearRGB();
			}
		}
	}

	bool FCellGraphBuilder::CollapsedBestGroupEdge(UDAFlowCellGraph* InGraph, int MinGroupArea, const FRandomStream& Random, FCellAreaLookup& AreaLookup) {
		int GroupAId;
		{
			SCOPE_CYCLE_COUNTER(STAT_CFColl_PrepNodeGroups);
			if (!AreaLookup.GetGroupWithLeastArea(Random, GroupAId)) {
				return false;
			}

			if (AreaLookup.GetGroupArea(GroupAId) > MinGroupArea) {
				// No need to merge any further
				return false;
			}
		}
		
		const FDAFlowCellGroupNode& GroupA = InGraph->GroupNodes[GroupAId];

		int GroupBId = -1;
		TArray<int> ConnectedGroups;
		{
			SCOPE_CYCLE_COUNTER(STAT_CFColl_PrepConnGroups);
			TArray<int> ConnectionsA = GroupA.Connections.Array();
			if (ConnectionsA.Num() == 0) {
				return false;
			}

			float BestArea = MAX_flt;
			int BestConnectedGroup = -1;
			for (const int ConnectedGroupID: ConnectionsA) {
				const float ConnectedGroupArea = AreaLookup.GetGroupArea(ConnectedGroupID);
				if (ConnectedGroupArea < BestArea) {
					BestArea = ConnectedGroupArea;
					BestConnectedGroup = ConnectedGroupID;
				}
			}
			GroupBId = BestConnectedGroup;
		}

		check(GroupBId != -1);
		
		{
			SCOPE_CYCLE_COUNTER(STAT_CFColl_Merge);
			check(GroupAId != GroupBId);
			MergeGroups(InGraph, GroupAId, GroupBId, AreaLookup);
		}
		return true;
	}

	void FCellGraphBuilder::AssignGroupPreviewLocations(UDAFlowCellGraph* InGraph, const FCellAreaLookup& InAreaLookup) {
		SCOPE_CYCLE_COUNTER(STAT_CFBuild_AssignPreviewLoc);
		TArray<int> GroupCenterNode;
		GroupCenterNode.SetNum(InGraph->GroupNodes.Num());
		for (int i = 0; i < GroupCenterNode.Num(); i++) {
			GroupCenterNode[i] = -1;
		}
			
		for (int GroupId = 0; GroupId < InGraph->GroupNodes.Num(); GroupId++) {
			FDAFlowCellGroupNode& GroupNode = InGraph->GroupNodes[GroupId];
			if (GroupNode.LeafNodes.Num() > 0) {
				int BestLeafId = -1;
				
				FVector2D Center = FVector2D::ZeroVector;
				{
					float Sum = 0;
					for (const int LeafNodeId : GroupNode.LeafNodes) {
						const float Area = InAreaLookup.GetLeafArea(LeafNodeId); 
						Center += InGraph->LeafNodes[LeafNodeId]->GetCenter() * Area;
						Sum += Area;
					}
					Center /= Sum;
				}

				float BestDistance = MAX_flt;;
				for (const int LeafNodeId : GroupNode.LeafNodes) {
					const UDAFlowCellLeafNode* LeafNode = InGraph->LeafNodes[LeafNodeId];
					const float DistanceToCenter = (Center - LeafNode->GetCenter()).Size();
					if (DistanceToCenter < BestDistance) {
						BestDistance = DistanceToCenter;
						BestLeafId = LeafNodeId;
					}
				}
				GroupNode.PreviewLocation = InGraph->LeafNodes[BestLeafId]->GetCenter();
				GroupCenterNode[GroupId] = BestLeafId;
			}
		}
	}

	void FCellGraphBuilder::GenerateEdgeList(UDAFlowCellGraph* CellGraph, TArray<FCellFlowGridEdgeInfo>& HalfEdges,
			const FFlowAbstractGraphQuery& LayoutGraphQuery, bool bHandleInactiveGroups) {
		TMap<FIntPoint, int32> CellHeights;
		TMap<FIntPoint, int32> CellGroups;

		for (const FDAFlowCellGroupNode& GroupNode : CellGraph->GroupNodes) {
			if (!GroupNode.IsActive() || !GroupNode.LayoutNodeID.IsValid()) continue;
			const UFlowAbstractNode* LayoutNode = LayoutGraphQuery.GetNode(GroupNode.LayoutNodeID);
			if (!LayoutNode) {
				continue;
			}
			
			if (!bHandleInactiveGroups && !LayoutNode->bActive) {
				// Do not handle this inactive node
				continue;
			}
			
			for (const int LeafNodeId : GroupNode.LeafNodes) {
				if (const UDAFlowCellLeafNodeGrid* GridLeafNode = Cast<UDAFlowCellLeafNodeGrid>(CellGraph->LeafNodes[LeafNodeId])) {
					for (int y = GridLeafNode->Location.Y; y < GridLeafNode->Location.Y + GridLeafNode->Size.Y; y++) {
						for (int x = GridLeafNode->Location.X; x < GridLeafNode->Location.X + GridLeafNode->Size.X; x++) {
							int32& HeightRef = CellHeights.FindOrAdd(FIntPoint(x, y));
							HeightRef = GridLeafNode->LogicalZ;

							int32& GroupRef = CellGroups.FindOrAdd(FIntPoint(x, y));
							GroupRef = GroupNode.GroupId;
						}
					}
				}
			}
		}
		
		TSet<FIntVector4> HandleEdgeVisited;
		auto HandleEdge = [&CellGroups, &CellHeights, &HalfEdges, &HandleEdgeVisited](int X, int Y, int Z, int dx, int dy) {
			const FIntPoint Coord(X, Y);
			const FIntPoint CoordNeighbor(X + dx, Y + dy);
			const int GroupBase = CellGroups[Coord];
			const int* GroupNextPtr = CellGroups.Find(CoordNeighbor);
			const int GroupNext = GroupNextPtr ? *GroupNextPtr : INDEX_NONE;
			if (HandleEdgeVisited.Contains(FIntVector4(Coord.X, Coord.Y, CoordNeighbor.X, CoordNeighbor.Y))) {
				return;
			}

			// Place the key in the other direction so the twin can skip it later
			HandleEdgeVisited.Add(FIntVector4(CoordNeighbor.X, CoordNeighbor.Y, Coord.X, Coord.Y));
		
			if (GroupBase != GroupNext) {
				const int EdgeIdx = HalfEdges.Num();
				const int TwinEdgeIdx = HalfEdges.Num() + 1;
			
				FCellFlowGridEdgeInfo Edge;
				Edge.Coord = Coord;
				Edge.TileGroup = GroupBase;
				Edge.HeightZ = CellHeights[Coord];
				Edge.EdgeIndex = EdgeIdx;
			
				FCellFlowGridEdgeInfo TwinEdge;
				TwinEdge.Coord = CoordNeighbor;
				TwinEdge.TileGroup = GroupNext;
				TwinEdge.HeightZ = (GroupNext != INDEX_NONE)
					? CellHeights[CoordNeighbor]
					: Edge.HeightZ;
				TwinEdge.EdgeIndex = TwinEdgeIdx;
			
				Edge.TwinIndex = TwinEdgeIdx;
				TwinEdge.TwinIndex = EdgeIdx;

				HalfEdges.Add(MoveTemp(Edge));
				HalfEdges.Add(MoveTemp(TwinEdge));
			}
		};
	
		for (const FDAFlowCellGroupNode& GroupNode : CellGraph->GroupNodes) {
			if (!GroupNode.IsActive() || !GroupNode.LayoutNodeID.IsValid()) continue;
			const UFlowAbstractNode* LayoutNode = LayoutGraphQuery.GetNode(GroupNode.LayoutNodeID);
			if (!LayoutNode) {
				continue;
			}
			
			if (!bHandleInactiveGroups && !LayoutNode->bActive) {
				// Do not handle this inactive node
				continue;
			}
			
			for (const int LeafNodeId : GroupNode.LeafNodes) {
				if (const UDAFlowCellLeafNodeGrid* GridLeafNode = Cast<UDAFlowCellLeafNodeGrid>(CellGraph->LeafNodes[LeafNodeId])) {
					const int Z = GridLeafNode->LogicalZ;
					for (int X = GridLeafNode->Location.X; X < GridLeafNode->Location.X + GridLeafNode->Size.X; X++) {
						HandleEdge(X, GridLeafNode->Location.Y, Z, 0, -1);
						HandleEdge(X, GridLeafNode->Location.Y + GridLeafNode->Size.Y - 1, Z, 0, 1);
					}
					for (int Y = GridLeafNode->Location.Y; Y < GridLeafNode->Location.Y + GridLeafNode->Size.Y; Y++) {
						HandleEdge(GridLeafNode->Location.X, Y, Z, -1, 0);
						HandleEdge(GridLeafNode->Location.X + GridLeafNode->Size.X - 1, Y, Z, 1, 0);
					}
				}
			}
		}
	}

	void FCellGraphBuilder::MergeGroups(UDAFlowCellGraph* InGraph, int GroupIdA, int GroupIdB, FCellAreaLookup& AreaLookup) {
		FDAFlowCellGroupNode& GroupA = InGraph->GroupNodes[GroupIdA];
		FDAFlowCellGroupNode& GroupB = InGraph->GroupNodes[GroupIdB];

		// Copy B to A and discard B
		for (const int GroupBLeafId : GroupB.LeafNodes) {
			GroupA.LeafNodes.Add(GroupBLeafId);
		}
		GroupB.LeafNodes.Reset();

		// Break all GroupB connections
		for (int ConnectedToB : GroupB.Connections) {
			InGraph->GroupNodes[ConnectedToB].Connections.Remove(GroupIdB);
			InGraph->GroupNodes[ConnectedToB].Connections.Add(GroupIdA);
			GroupA.Connections.Add(ConnectedToB);
		}
		GroupB.Connections.Reset();

		// Remove internal links (merged nodes do not connect to each other)
		TArray<int> ValidConnectionsA;
		for (int ConnectedCellID : GroupA.Connections) {
			if (!GroupA.LeafNodes.Contains(ConnectedCellID)) {
				ValidConnectionsA.Add(ConnectedCellID);
			}
		}
		GroupA.Connections = TSet(ValidConnectionsA);

		// Copy GroupB's area over ot A and then clear it on B
		const float MergedArea = AreaLookup.GetGroupArea(GroupIdA) + AreaLookup.GetGroupArea(GroupIdB); 
		AreaLookup.SetGroupArea(GroupIdA, MergedArea);
		AreaLookup.SetGroupArea(GroupIdB, 0);
	}


	////////////////////////////// FCellAreaLookup //////////////////////////////
	void FCellAreaLookup::Init(UDAFlowCellGraph* InGraph) {
		LeafAreas.SetNum(InGraph->LeafNodes.Num());
		for (int LeafId = 0; LeafId < InGraph->LeafNodes.Num(); LeafId++) {
			LeafAreas[LeafId] = InGraph->LeafNodes[LeafId]->GetArea();
		}

		GroupAreas.SetNum(InGraph->GroupNodes.Num());
		for (int GroupId = 0; GroupId < InGraph->GroupNodes.Num(); GroupId++) {
			FDAFlowCellGroupNode& GroupNode = InGraph->GroupNodes[GroupId];
			if (GroupNode.Connections.Num() == 0 || GroupNode.LeafNodes.Num() == 0) {
				continue;
			}
			
			float& GroupArea = GroupAreas[GroupId];
			GroupArea = 0;
			const TSet<int>& LeafNodeIds = GroupNode.LeafNodes;
			for (const int LeafId : LeafNodeIds) {
				GroupArea += LeafAreas[LeafId];
			}
			if (GroupArea > 0) {
				TArray<int>& Groups = ActiveGroupIdsByArea.FindOrAdd(GroupArea);
				Groups.Add(GroupId);
			}
		}
	}

	void FCellAreaLookup::SetGroupArea(int GroupId, float NewArea) {
		const float OldArea = GroupAreas[GroupId];
		GroupAreas[GroupId] = NewArea;

		TArray<int>& OldAreaGroups = ActiveGroupIdsByArea.FindOrAdd(OldArea);
		OldAreaGroups.Remove(GroupId);
		if (OldAreaGroups.Num() == 0) {
			ActiveGroupIdsByArea.Remove(OldArea);
		}

		if (NewArea > 0) {
			TArray<int>& NewAreaGroups = ActiveGroupIdsByArea.FindOrAdd(NewArea);
			check(!NewAreaGroups.Contains(GroupId));
			NewAreaGroups.Add(GroupId);
		}
	}

	bool FCellAreaLookup::GetGroupWithLeastArea(const FRandomStream& InRandom, int& OutGroupId) const {
		float BestArea = MAX_flt;
		bool bFound = false;
		for (const auto& Entry : ActiveGroupIdsByArea) {
			const float Area = Entry.Key;
			const TArray<int>& GroupIds = Entry.Value;
			if (GroupIds.Num() == 0) {
				continue;
			}

			if (Area < BestArea) {
				BestArea = Area;
				bFound = true;
			}
		}

		if (!bFound) {
			OutGroupId = -1;
			return false;
		}

		const TArray<int>& Candidates = ActiveGroupIdsByArea[BestArea];
		OutGroupId = Candidates[InRandom.RandRange(0, Candidates.Num() - 1)];
		return true;
	}
}

