//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/Tasks/CellFlowLayoutTaskCreateCellsBase.h"

#include "Frameworks/Flow/Domains/LayoutGraph/Core/FlowAbstractGraphUtils.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/CellFlowLayoutGraph.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowLib.h"

////////////////////////////////////// UCellFlowLayoutTaskCreateCells //////////////////////////////////////

UCellFlowLayoutTaskCreateCellsBase::UCellFlowLayoutTaskCreateCellsBase() {
    InputConstraint = EFlowExecTaskInputConstraint::NoInput;
}

void UCellFlowLayoutTaskCreateCellsBase::Execute(const FFlowExecutionInput& Input, const FFlowTaskExecutionSettings& InExecSettings, FFlowExecutionOutput& Output) const {
    check(Input.IncomingNodeOutputs.Num() == 0);
    
    if (!Input.Random) {
        Output.ExecutionResult = EFlowTaskExecutionResult::FailHalt;
        Output.ErrorMessage = "Internal Error. Invalid State";
        return;
    }
    
    Output.State = MakeShareable(new FFlowExecNodeState);
    
    // Build the graph object
    const FRandomStream& Random = *Input.Random;

    // Generate the cell graph
    UDAFlowCellGraph* CellGraph = NewObject<UDAFlowCellGraph>();
    {
        using namespace DA;
        
        // Generate Grid Cells 
        GenerateCellsImpl(CellGraph, Random, Output.State);
		
        FCellAreaLookup AreaLookup;
        AreaLookup.Init(CellGraph);
	
        FCellGraphBuilder::CollapseEdges(CellGraph, MinGroupArea, Random, AreaLookup);
        FCellGraphBuilder::AssignGroupColors(CellGraph);
        FCellGraphBuilder::AssignGroupPreviewLocations(CellGraph, AreaLookup);
    }

    // Generate the layout graph
    UCellFlowLayoutGraph* LayoutGraph = NewObject<UCellFlowLayoutGraph>();
    
    // Use a random seed that's combined with the dungeon UID, This avoids node UID collision with different dungeons on the same map 
    const FRandomStream NamingSchemeRandomStream(FFlowAbstractGraphUtils::CreateUidRandomSeed(Input.Random, Input.Dungeon));
    CreateGraphs(LayoutGraph, CellGraph, NamingSchemeRandomStream);

    // Create a new state, since this will our first node
    Output.State->SetStateObject(UFlowAbstractGraphBase::StateTypeID, LayoutGraph);
    Output.State->SetStateObject(UDAFlowCellGraph::StateTypeID, CellGraph);
    
    Output.ExecutionResult = EFlowTaskExecutionResult::Success;
}

void UCellFlowLayoutTaskCreateCellsBase::CreateGraphs(UCellFlowLayoutGraph* LayoutGraph, UDAFlowCellGraph* CellGraph, const FRandomStream& InRandom) {
    TMap<int32, FGuid> GroupToFlowNode;
    TMap<FGuid, UFlowAbstractNode*> FlowNodes;
    for (FDAFlowCellGroupNode& GroupNode : CellGraph->GroupNodes) {
        if (GroupNode.IsActive()) {
            const FVector Coord = FVector(GroupNode.PreviewLocation, 0);
            
            // Create a new Layout node
            UFlowAbstractNode* LayoutNode = LayoutGraph->CreateNode(InRandom);
            LayoutNode->Coord = Coord;

            // Save the ID of the layout node in the cell group node
            GroupNode.LayoutNodeID = LayoutNode->NodeId;

            // Cache to lookup tables
            GroupToFlowNode.Add(GroupNode.GroupId, LayoutNode->NodeId);
            FlowNodes.Add(LayoutNode->NodeId, LayoutNode);
        }
    }

    auto GetFlowNodeFromGroupId = [&](int GroupId) -> UFlowAbstractNode* {
        const FGuid* FlowNodeIdPtr = GroupToFlowNode.Find(GroupId);
        return (FlowNodeIdPtr == nullptr) ? nullptr : FlowNodes[*FlowNodeIdPtr];
    };
    
    for (const FDAFlowCellGroupNode& GroupNode : CellGraph->GroupNodes) {
        if (GroupNode.IsActive()) {
            if (const UFlowAbstractNode* SrcNode = GetFlowNodeFromGroupId(GroupNode.GroupId)) {
                for (const int32 OtherGroupId : GroupNode.Connections) {
                    if (GroupNode.GroupId < OtherGroupId) {
                        if (const UFlowAbstractNode* DstNode = GetFlowNodeFromGroupId(OtherGroupId)) {
                            LayoutGraph->CreateLink(SrcNode->NodeId, DstNode->NodeId, InRandom);
                        }
                    }
                }
            }
        }
    }
}

bool UCellFlowLayoutTaskCreateCellsBase::GetParameter(const FString& InParameterName, FDAAttribute& OutValue) {
    //FLOWTASKATTR_GET_VECTOR(WorldSize);

    return false;
    
}

bool UCellFlowLayoutTaskCreateCellsBase::SetParameter(const FString& InParameterName, const FDAAttribute& InValue) {
    //FLOWTASKATTR_SET_VECTORF(WorldSize)

    return false;
    
}

bool UCellFlowLayoutTaskCreateCellsBase::SetParameterSerialized(const FString& InParameterName, const FString& InSerializedText) {
    //FLOWTASKATTR_SET_PARSE_VECTORF(WorldSize)
    
    return false;
}

