//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Builders/Grid/GridDungeonModel.h"

#include "Builders/Grid/GridDungeonConfig.h"
#include "Core/Layout/DungeonLayoutData.h"

#include "GeomTools.h"

void UGridDungeonModel::BuildCellLookup() {
    CellIdToIndexLookup.Reset();
    FGridDungeonCell* CellArray = Cells.GetData();
    for (int CellIdx = 0; CellIdx < Cells.Num(); CellIdx++) {
        if (FGridDungeonCell* Cell = CellArray + CellIdx) {
            CellIdToIndexLookup.Add(Cell->Id, CellIdx);
        }
    }
}

FGridDungeonCell* UGridDungeonModel::GetCell(int32 Id) {
    const int32* IndexPtr = CellIdToIndexLookup.Find(Id);
    if (!IndexPtr || !Cells.IsValidIndex(*IndexPtr)) {
        return nullptr;
    }
    return &Cells[*IndexPtr];
}

const FGridDungeonCell* UGridDungeonModel::GetCell(int32 Id) const {
    const int32* IndexPtr = CellIdToIndexLookup.Find(Id);
    if (!IndexPtr || !Cells.IsValidIndex(*IndexPtr)) {
        return nullptr;
    }
    return &Cells[*IndexPtr];
}

bool UGridDungeonModel::ContainsStairAtLocation(const FIntPoint& P) {
    return ContainsStairAtLocation(P.X, P.Y);
}

bool UGridDungeonModel::ContainsStairAtLocation(int x, int y) {
    for (const auto& Stair : Stairs) {
        if (Stair.IPosition.X == x && Stair.IPosition.Y == y) {
            return true;
        }
    }
    return false;
}

bool UGridDungeonModel::ContainsStairBetweenCells(int32 CellIdA, int32 CellIdB) {
    if (CellStairsLookup.Contains(CellIdA)) {
        for (const FGridDungeonStairInfo& stair : CellStairsLookup[CellIdA]) {
            if (stair.ConnectedToCell == CellIdB) {
                return true;
            }
        }
    }

    if (CellStairsLookup.Contains(CellIdB)) {
        for (const FGridDungeonStairInfo& stair : CellStairsLookup[CellIdB]) {
            if (stair.ConnectedToCell == CellIdA) {
                return true;
            }
        }
    }

    return false;
}

FGridDungeonCellInfo UGridDungeonModel::GetGridCellLookup(int x, int y) const {
    if (!GridCellInfoLookup.Contains(x) || !GridCellInfoLookup[x].Contains(y)) {
        return FGridDungeonCellInfo();
    }
    return GridCellInfoLookup[x][y];
}

FGridDungeonCellInfo UGridDungeonModel::GetGridCellLookup(const FIntPoint& P) const {
    return GetGridCellLookup(P.X, P.Y);
}

void UGridDungeonModel::PostBuildCleanup() {
}

void UGridDungeonModel::Reset() {
    Cells.Reset();
    Clusters.Reset();
    Doors.Reset();
    Stairs.Reset();
    CellStairsLookup.Reset();
    CellIdToIndexLookup.Reset();
    GridCellInfoLookup.Reset();
    DoorManager = FGridDungeonDoorManager();
    BuildState = DungeonModelBuildState::Initial;
}

void UGridDungeonModel::GenerateLayoutData(const UDungeonConfig* InConfig, FDungeonLayoutData& OutLayout) const {
    Super::GenerateLayoutData(InConfig, OutLayout);
    OutLayout.FloorSettings = CreateFloorSettings(InConfig);
    OutLayout.Bounds.Init();
    if (const UGridDungeonConfig* GridConfig = Cast<UGridDungeonConfig>(InConfig)) {
        const FVector& GridSize = GridConfig->GridCellSize;
        // Write out the cluster boundaries
        for (const auto& ClusterEntry : Clusters) {
            FDungeonLayoutDataChunkInfo& OutChunkShape = OutLayout.ChunkShapes.AddDefaulted_GetRef();
            const FGridDungeonCellClusterInfo& ClusterInfo = ClusterEntry.Value;
            for (const int32 CellId : ClusterInfo.CellIds) {
                if (const FGridDungeonCell* Cell = GetCell(CellId)) {
                    FDABoundsShapeConvexPoly& OutCellPoly = OutChunkShape.ConvexPolys.AddDefaulted_GetRef();
                    OutCellPoly.Height = Cell->Bounds.Height() * GridSize.Z;
                    OutCellPoly.Transform = FTransform::Identity;
                    FVector CellLocation = FVector(Cell->Bounds.Location) * GridSize;
                    FVector CellSize = FVector(Cell->Bounds.Size) * GridSize;
                    CellSize.Z = ClusterInfo.GridZ * GridSize.Z;
                    FBox CellBox(CellLocation, CellLocation + CellSize);
                    FDABoundsShapeCollision::ConvertBoxToConvexPoly(CellBox, OutCellPoly);
                    OutLayout.Bounds += CellBox;
                }
            }
        }

        const float DoorOpeningSize = GridConfig->GridCellSize.X * 0.9f;
        const float DoorOcclusionSize = GridConfig->GridCellSize.X * 0.9f;

        // Populate the stair info
        for (const FGridDungeonStairInfo& StairInfo : Stairs) {
            FDungeonLayoutDataStairItem& OutStair = OutLayout.Stairs.AddDefaulted_GetRef();
            OutStair.WorldTransform = FTransform(StairInfo.Rotation, StairInfo.Position);
            OutStair.Width = DoorOpeningSize;
        }

        // Populate the door info
        for (const FGridDungeonCellDoor& GridDoorInfo : Doors) {
            FDungeonLayoutDataDoorItem& OutDoorInfo = OutLayout.Doors.AddDefaulted_GetRef();
            OutDoorInfo.Width = DoorOpeningSize;
            OutDoorInfo.DoorOcclusionThickness = DoorOcclusionSize;
            
            FVector Tile0 = FVector(GridDoorInfo.AdjacentTiles[0]) + FVector(0.5f, 0.5f, 0);
            FVector Tile1 = FVector(GridDoorInfo.AdjacentTiles[1]) + FVector(0.5f, 0.5f, 0);
            Tile0.Z = FMath::Max(Tile0.Z, Tile1.Z);
            Tile1.Z = FMath::Max(Tile0.Z, Tile1.Z);
            
            FVector WorldMidPoint = (Tile0 + Tile1) * 0.5f * GridConfig->GridCellSize;
            OutDoorInfo.WorldTransform = FTransform(
                GridDoorInfo.AdjacentTiles[0].X == GridDoorInfo.AdjacentTiles[1].X
                    ? FRotator(0, 0, 0)
                    : FRotator(0, 90, 0),
                WorldMidPoint);
        }
    }
}

FDungeonFloorSettings UGridDungeonModel::CreateFloorSettings(const UDungeonConfig* InConfig) const {
    FDungeonFloorSettings Settings = {};
    if (const UGridDungeonConfig* GridConfig = Cast<UGridDungeonConfig>(InConfig)) {
        Settings.FloorHeight = GridConfig->GridCellSize.Z;
        Settings.FloorCaptureHeight = Settings.FloorHeight - 1;
    }
    int32 MinCellZ{};
    int32 MaxCellZ{};
    if (Cells.Num() > 0) {
        MinCellZ = MaxCellZ = Cells[0].Bounds.Location.Z;
        for (const FGridDungeonCell& Cell : Cells) {
            MinCellZ = FMath::Min(MinCellZ, Cell.Bounds.Location.Z);
            MaxCellZ = FMath::Max(MaxCellZ, Cell.Bounds.Location.Z);
        }
    }
    Settings.FloorLowestIndex = MinCellZ;
    Settings.FloorHighestIndex = MaxCellZ;
    return Settings;   
}

bool operator==(const FGridDungeonCellDoor& A, const FGridDungeonCellDoor& B) {
    return A.AdjacentCells[0] == B.AdjacentCells[0]
        && A.AdjacentCells[1] == B.AdjacentCells[1]
        && A.AdjacentTiles[0] == B.AdjacentTiles[0]
        && A.AdjacentTiles[1] == B.AdjacentTiles[1];
}

