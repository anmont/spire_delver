//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Voxel/Meshing/MarchingCube/TransVoxelMeshBuilder.h"

#include "Frameworks/Voxel/Meshing/MarchingCube/TransVoxelLookup.h"

namespace DA {
	struct DUNGEONARCHITECTRUNTIME_API FVoxelCellCoords {
		/** Indices of the voxel in the block */
		FIntVector WorldCoords[8];
		FIntVector LocalIndexCoords[8];
	};

	struct DUNGEONARCHITECTRUNTIME_API FVoxelCellDensities {
		/** The densities at each of the cell's 8 corners */
		float Densities[8];
	};
}

namespace FDATransVoxelUtils {
	FORCEINLINE void GetCellCoords(const DA::FChunkDensityData& InData, int32 DX, int32 DY, int32 DZ, DA::FVoxelCellCoords& OutCellCoords)
	{
		// LOD Increment
		const int32 I = 1 << InData.GetLOD();
		const FIntVector Origin = InData.GetOrigin();
		const int32 X = Origin.X + DX * I;
		const int32 Y = Origin.Y + DY * I;
		const int32 Z = Origin.Z + DZ * I;

		OutCellCoords.WorldCoords[0] = FIntVector(X + 0, Y + 0, Z + 0);
		OutCellCoords.WorldCoords[1] = FIntVector(X + I, Y + 0, Z + 0);
		OutCellCoords.WorldCoords[2] = FIntVector(X + 0, Y + I, Z + 0);
		OutCellCoords.WorldCoords[3] = FIntVector(X + I, Y + I, Z + 0);
		OutCellCoords.WorldCoords[4] = FIntVector(X + 0, Y + 0, Z + I);
		OutCellCoords.WorldCoords[5] = FIntVector(X + I, Y + 0, Z + I);
		OutCellCoords.WorldCoords[6] = FIntVector(X + 0, Y + I, Z + I);
		OutCellCoords.WorldCoords[7] = FIntVector(X + I, Y + I, Z + I);

		OutCellCoords.LocalIndexCoords[0] = FIntVector(DX + 0, DY + 0, DZ + 0);
		OutCellCoords.LocalIndexCoords[1] = FIntVector(DX + 1, DY + 0, DZ + 0);
		OutCellCoords.LocalIndexCoords[2] = FIntVector(DX + 0, DY + 1, DZ + 0);
		OutCellCoords.LocalIndexCoords[3] = FIntVector(DX + 1, DY + 1, DZ + 0);
		OutCellCoords.LocalIndexCoords[4] = FIntVector(DX + 0, DY + 0, DZ + 1);
		OutCellCoords.LocalIndexCoords[5] = FIntVector(DX + 1, DY + 0, DZ + 1);
		OutCellCoords.LocalIndexCoords[6] = FIntVector(DX + 0, DY + 1, DZ + 1);
		OutCellCoords.LocalIndexCoords[7] = FIntVector(DX + 1, DY + 1, DZ + 1);
	}


	FORCEINLINE void GetCellDensities(const DA::FChunkDensityData& InData, const FIntVector& InDeltaCoord, DA::FVoxelCellDensities& OutCellDensities)
	{
		static const TArray<FIntVector> LocalCornerOffsets = {
			FIntVector(0, 0, 0),
			FIntVector(1, 0, 0),
			FIntVector(0, 1, 0),
			FIntVector(1, 1, 0),
			FIntVector(0, 0, 1),
			FIntVector(1, 0, 1),
			FIntVector(0, 1, 1),
			FIntVector(1, 1, 1)
		};
	
		for (int i = 0; i < 8; i++) {
			const FIntVector& I = InDeltaCoord + LocalCornerOffsets[i];
			OutCellDensities.Densities[i] = InData.Get(I);
			check(!FMath::IsNaN(OutCellDensities.Densities[i]));
		}
	}


	FORCEINLINE void GetCutPoint(float D0, float D1, const FIntVector& Coord0, const FIntVector& Coord1, float VoxelSize, FVector& OutP, float& OutT)
	{
		// We are at the highest level of detail. Grab the point from the adjacent points
		const FVector P0 = FVector(Coord0.X, Coord0.Y, Coord0.Z) * VoxelSize;
		const FVector P1 = FVector(Coord1.X, Coord1.Y, Coord1.Z) * VoxelSize;
		OutT = D1 / (D1 - D0);
		OutP = P0 * OutT + P1 * (1 - OutT);
	}

	float Sample(const DA::FChunkDensityData& InData, const FVector& LP) {
		int32 X0, Y0, Z0, X1, Y1, Z1;
		float DX, DY, DZ;
		{
			X0 = FMath::FloorToInt(LP.X);
			Y0 = FMath::FloorToInt(LP.Y);
			Z0 = FMath::FloorToInt(LP.Z);

			X1 = X0 + 1;
			Y1 = Y0 + 1;
			Z1 = Z0 + 1;

			const int32 MinIdx = InData.GetMinIndex();
			const int32 MaxIdx = InData.GetMaxIndex();
			X1 = FMath::Clamp(X1, MinIdx, MaxIdx);
			Y1 = FMath::Clamp(Y1, MinIdx, MaxIdx);
			Z1 = FMath::Clamp(Z1, MinIdx, MaxIdx);

			DX = LP.X - X0;
			DY = LP.Y - Y0;
			DZ = LP.Z - Z0;
		}
	
		const float D000 = InData.Get(FIntVector(X0, Y0, Z0));
		const float D100 = InData.Get(FIntVector(X1, Y0, Z0));
		const float D010 = InData.Get(FIntVector(X0, Y1, Z0));
		const float D110 = InData.Get(FIntVector(X1, Y1, Z0));
	
		const float D001 = InData.Get(FIntVector(X0, Y0, Z1));
		const float D101 = InData.Get(FIntVector(X1, Y0, Z1));
		const float D011 = InData.Get(FIntVector(X0, Y1, Z1));
		const float D111 = InData.Get(FIntVector(X1, Y1, Z1));

		return FMath::Lerp(
			FMath::Lerp(
				FMath::Lerp(D000, D100, DX),
				FMath::Lerp(D010, D110, DX), DY),
			FMath::Lerp(
				FMath::Lerp(D001, D101, DX),
				FMath::Lerp(D011, D111, DX), DY), DZ);
	}

	FVector GetNormalAt(const DA::FChunkDensityData& InData, const FVector& P) {
		FVector Normal;
	
		constexpr float Delta = 1.0f;
		Normal.X = Sample(InData, FVector(P.X - Delta, P.Y, P.Z)) - Sample(InData, FVector(P.X + Delta, P.Y, P.Z));
		Normal.Y = Sample(InData, FVector(P.X, P.Y - Delta, P.Z)) - Sample(InData, FVector(P.X, P.Y + Delta, P.Z));
		Normal.Z = Sample(InData, FVector(P.X, P.Y, P.Z - Delta)) - Sample(InData, FVector(P.X, P.Y, P.Z + Delta));
		Normal.Normalize();

		return Normal;
	}
}

void DA::FTransVoxelMeshBuilder::GenerateMesh(const FChunkDensityData& InData, float InVoxelSize, FVoxelGeometry& OutMeshData) {
	using namespace UE::Geometry;
	
	struct FCellGeometry {
		int32 VertexBufferIndex[12];
		int32 CreatedIndices[12];
	};

	constexpr int32 Start = 0;
	const int32 End = InData.GetNumCellsPerChunk();

	const int32 NumVoxels = End - Start;

	TArray<FCellGeometry> VoxelDecks[2];

	const int32 DeckSize = NumVoxels * NumVoxels;
	VoxelDecks[0].AddUninitialized(DeckSize);
	VoxelDecks[1].AddUninitialized(DeckSize);
	int32 ActiveDeckIndex = 0;

	FVoxelCellCoords CellCoords;
	FVoxelCellDensities CellDensities;

	for (int iz = 0; iz < NumVoxels; iz++) {
		const uint8 CanMoveZ = (iz == 0 ? 0 : 1) << 2;
		for (int iy = 0; iy < NumVoxels; iy++) {
			const uint8 CanMoveY = (iy == 0 ? 0 : 1) << 1;
			for (int ix = 0; ix < NumVoxels; ix++) {
				const uint8 CanMoveX = (ix == 0) ? 0 : 1;

				FDATransVoxelUtils::GetCellCoords(InData, ix, iy, iz, CellCoords);
				FDATransVoxelUtils::GetCellDensities(InData, FIntVector(ix, iy, iz), CellDensities);

				uint32 CubeIndex = 0;
				if (CellDensities.Densities[0] < 0) CubeIndex |= 0x01;
				if (CellDensities.Densities[1] < 0) CubeIndex |= 0x02;
				if (CellDensities.Densities[2] < 0) CubeIndex |= 0x04;
				if (CellDensities.Densities[3] < 0) CubeIndex |= 0x08;
				if (CellDensities.Densities[4] < 0) CubeIndex |= 0x10;
				if (CellDensities.Densities[5] < 0) CubeIndex |= 0x20;
				if (CellDensities.Densities[6] < 0) CubeIndex |= 0x40;
				if (CellDensities.Densities[7] < 0) CubeIndex |= 0x80;

				if (CubeIndex == 0 || CubeIndex == 255) {
					continue;
				}

				const uint8 ValidMovementMask = CanMoveX | CanMoveY | CanMoveZ;

				const uint8 CellClass = RegularCellClass[CubeIndex];
				const FRegularCellData& TriangulationData = RegularCellData[CellClass];
				const uint16* VertexData = RegularVertexData[CubeIndex];

				FCellGeometry& CurrentCellGeometry = VoxelDecks[ActiveDeckIndex][ix + iy * NumVoxels];
				const uint32 NumVertices = TriangulationData.GetVertexCount();
				for (uint32 vi = 0; vi < NumVertices; vi++) {
					const uint16 EdgeCode = VertexData[vi];

					const uint16 CornerIndex0 = (EdgeCode >> 4) & 0x0F;
					const uint16 CornerIndex1 = EdgeCode & 0x0F;

					const uint8 MappingCode = (EdgeCode >> 8);
					const uint8 MovementMask = (MappingCode >> 4) & 0x0F;
					const uint8 VertexLocalIndex = MappingCode & 0x0F;

					const bool bCreateVertex = (MovementMask & ValidMovementMask) != MovementMask;

					int32 VertexBufferIndex = -1;
					if (bCreateVertex) {
						// Create a new vertex here
						float D0 = CellDensities.Densities[CornerIndex0];
						float D1 = CellDensities.Densities[CornerIndex1];
						FIntVector Coord0 = CellCoords.WorldCoords[CornerIndex0];
						FIntVector Coord1 = CellCoords.WorldCoords[CornerIndex1];
						//FVector P;
						FVertexInfo Vertex;
						
						float t = 0;

						FDATransVoxelUtils::GetCutPoint(D0, D1, Coord0, Coord1, InVoxelSize, Vertex.Position, t);
						Vertex.UV = FVector2f::ZeroVector;
					
						FIntVector GC0 = CellCoords.LocalIndexCoords[CornerIndex0];
						FIntVector GC1 = CellCoords.LocalIndexCoords[CornerIndex1];
						FVector GridLocation = FVector(GC0) * t + FVector(GC1) * (1 - t);

						Vertex.Normal = FVector3f(FDATransVoxelUtils::GetNormalAt(InData, GridLocation));

						VertexBufferIndex = OutMeshData.Vertices.Add(Vertex);
						if (MovementMask == 8) {
							CurrentCellGeometry.CreatedIndices[VertexLocalIndex] = VertexBufferIndex;
						}

					}
					else {
						// Reuse the vertex from the previous frame
						int32 px = ix;
						int32 py = iy;
						int32 pz = ActiveDeckIndex;

						if (MovementMask & 0x01) px = FMath::Max(0, px - 1);
						if (MovementMask & 0x02) py = FMath::Max(0, py - 1);
						if (MovementMask & 0x04) {
							pz = (pz + 1) % 2;
						}
						const FCellGeometry& PreviousCellGeometry = VoxelDecks[pz][px + py * NumVoxels];
						VertexBufferIndex = PreviousCellGeometry.CreatedIndices[VertexLocalIndex];
					}

					CurrentCellGeometry.VertexBufferIndex[vi] = VertexBufferIndex;
				}

				// Build the triangles for this cell
				const int32 NumTriangles = TriangulationData.GetTriangleCount();
				for (int t = 0; t < NumTriangles; t++) {
					const int32 i0 = TriangulationData.vertexIndex[t * 3 + 0];
					const int32 i1 = TriangulationData.vertexIndex[t * 3 + 1];
					const int32 i2 = TriangulationData.vertexIndex[t * 3 + 2];

					const int32 v0 = CurrentCellGeometry.VertexBufferIndex[i0];
					const int32 v1 = CurrentCellGeometry.VertexBufferIndex[i1];
					const int32 v2 = CurrentCellGeometry.VertexBufferIndex[i2];

					FIndex3i Triangle(v0, v1, v2);
					OutMeshData.Triangles.Add(Triangle);
				}
			}
		}

		ActiveDeckIndex = (ActiveDeckIndex + 1) % 2;
	}
}

