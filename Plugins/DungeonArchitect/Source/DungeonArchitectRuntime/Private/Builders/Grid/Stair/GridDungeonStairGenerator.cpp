//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Builders/Grid/Stair/GridDungeonStairGenerator.h"

#include "Builders/Grid/GridDungeonBuilder.h"
#include "Builders/Grid/GridDungeonBuilderLib.h"
#include "Builders/Grid/GridDungeonModel.h"

/////////////////////// Base Stair Generator ///////////////////////
void UGridDungeonBuilderStairGeneratorBase::GenerateDungeonHeights(UGridDungeonModel* GridModel, UGridDungeonBuilder* GridBuilder, bool bFixHeights) const {
    using namespace GridDungeonBuilderImpl;
    // build the adjacency graph in memory
    if (GridModel->Cells.Num() == 0) return;
    TMap<int32, FCellHeightNode> CellHeightNodes;

    TSet<int32> Visited;
    for (const FGridDungeonCell& StartCell : GridModel->Cells) {
        if (Visited.Contains(StartCell.Id)) {
            continue;
        }
        
        TArray<CellHeightFrameInfo> Stack;
        Stack.Push(CellHeightFrameInfo(StartCell.Id, StartCell.Bounds.Location.Z));

        while (Stack.Num() > 0) {
            CellHeightFrameInfo top = Stack.Pop();
            if (Visited.Contains(top.CellId)) continue;
            Visited.Add(top.CellId);

            FGridDungeonCell* Cell = GridModel->GetCell(top.CellId);
            if (!Cell) continue;

            bool applyHeightVariation = (Cell->Bounds.Size.X > 1 && Cell->Bounds.Size.Y > 1);
            applyHeightVariation &= (Cell->CellType != EGridDungeonCellType::Room && Cell->CellType != EGridDungeonCellType::CorridorPadding);
            applyHeightVariation &= !Cell->UserDefined;

            float VariationProbability = FMath::Clamp(HeightVariationProbability, 0.0f, 1.0f);
            if (applyHeightVariation) {
                FRandomStream& Random = GridBuilder->GetRandomStreamRef();
                float RandomValue01 = Random.FRand();
                if (RandomValue01 < VariationProbability / 2.0f) {
                    top.CurrentHeight--;
                }
                else if (RandomValue01 < VariationProbability) {
                    top.CurrentHeight++;
                }
            }
            if (Cell->UserDefined) {
                top.CurrentHeight = Cell->Bounds.Location.Z;
            }

            FCellHeightNode node;
            node.CellId = Cell->Id;
            node.Height = top.CurrentHeight;
            node.MarkForIncrease = false;
            node.MarkForDecrease = false;
            CellHeightNodes.Add(node.CellId, node);

            // Add the child nodes
            for (int32 childId : Cell->AdjacentCells) {
                if (Visited.Contains(childId)) continue;
                Stack.Push(CellHeightFrameInfo(childId, top.CurrentHeight));
            }
        }
    }

    if (bFixHeights) {
        // Fix the dungeon heights
        const int32 FIX_MAX_TRIES = 50; // TODO: Move to gridConfig
        int32 fixIterations = 0;
        while (fixIterations < FIX_MAX_TRIES && FixDungeonCellHeights(GridModel, CellHeightNodes)) {
            fixIterations++;
        }
    }

    // Assign the calculated heights
    for (FGridDungeonCell& cell : GridModel->Cells) {
        if (CellHeightNodes.Contains(cell.Id)) {
            const FCellHeightNode& node = CellHeightNodes[cell.Id];
            cell.Bounds.Location.Z = node.Height;
        }
    }
}

bool UGridDungeonBuilderStairGeneratorBase::FixDungeonCellHeights(UGridDungeonModel* GridModel,
        TMap<int32, GridDungeonBuilderImpl::FCellHeightNode>& CellHeightNodes, const TSet<TPair<int32, int32>>& ClampedAdjacentNodes) const {
    using namespace GridDungeonBuilderImpl;
    bool bContinueIteration = false;
    if (GridModel->Cells.Num() == 0) return bContinueIteration;

    TSet<int32> Visited;
    TArray<int32> Stack;
    const FGridDungeonCell& RootCell = GridModel->Cells[0];
    Stack.Push(RootCell.Id);
    while (Stack.Num() > 0) {
        int32 cellId = Stack.Pop();
        if (Visited.Contains(cellId)) continue;
        Visited.Add(cellId);

        FGridDungeonCell* cell = GridModel->GetCell(cellId);
        if (!cell) continue;

        if (!CellHeightNodes.Contains(cellId)) continue;
        FCellHeightNode& heightNode = CellHeightNodes[cellId];

        heightNode.MarkForIncrease = false;
        heightNode.MarkForDecrease = false;

        // Check if the adjacent cells have unreachable heights
        for (int32 childId : cell->AdjacentCells) {
            FGridDungeonCell* childCell = GridModel->GetCell(childId);
            if (!childCell || !CellHeightNodes.Contains(childId)) continue;
            FCellHeightNode& childHeightNode = CellHeightNodes[childId];
            int32 heightDifference = FMath::Abs(childHeightNode.Height - heightNode.Height);
            int MaxAllowedHeight = MaxAllowedStairHeight;
            if (ClampedAdjacentNodes.Contains({ cell->Id, childId })) {
                MaxAllowedHeight = 0;
            }
            
            bool bNotReachable = (heightDifference > MaxAllowedHeight);

            if (bNotReachable) {
                if (heightNode.Height > childHeightNode.Height) {
                    heightNode.MarkForDecrease = true;
                }
                else {
                    heightNode.MarkForIncrease = true;
                }
                break;
            }
        }

        // Add the child nodes
        for (int32 childId : cell->AdjacentCells) {
            if (Visited.Contains(childId)) continue;
            Stack.Push(childId);
        }
    }


    TArray<int32> HeightCellIds;
    CellHeightNodes.GenerateKeyArray(HeightCellIds);
    bool bHeightChanged = false;
    for (int32 cellId : HeightCellIds) {
        FCellHeightNode& heightNode = CellHeightNodes[cellId];
        if (heightNode.MarkForDecrease) {
            heightNode.Height--;
            bHeightChanged = true;
            heightNode.MarkForDecrease = false;
        }
        else if (heightNode.MarkForIncrease) {
            heightNode.Height++;
            bHeightChanged = true;
            heightNode.MarkForIncrease = false;
        }
    }

    // Iterate this function again if the height was changed in this step
    bContinueIteration = bHeightChanged;
    return bContinueIteration;
}

