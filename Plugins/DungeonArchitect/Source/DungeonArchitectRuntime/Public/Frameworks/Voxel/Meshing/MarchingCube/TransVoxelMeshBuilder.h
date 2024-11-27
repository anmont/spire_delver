//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Frameworks/Voxel/Data/ChunkDensityData.h"
#include "Frameworks/Voxel/Meshing/Common/VoxelMeshStructures.h"

namespace DA {
	class FTransVoxelMeshBuilder {
	public:
		static void GenerateMesh(const FChunkDensityData& InData, float InVoxelSize, FVoxelGeometry& OutMeshData);
	};
}

