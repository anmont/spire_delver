//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Frameworks/Voxel/Data/ChunkDensityData.h"

#include "Components/DynamicMeshComponent.h"
#include "DungeonVoxelChunkComponent.generated.h"

struct FDAVoxelData {
	uint8 Density;
};

UCLASS()
class DUNGEONARCHITECTRUNTIME_API UDungeonVoxelChunkComponent : public UDynamicMeshComponent {
	GENERATED_BODY()
public:
	virtual void Serialize(FArchive& Ar) override;
	
	void Generate(const DA::FChunkDensityData& InData);

private:
	void SaveData();
	void LoadData();

private:
	UPROPERTY()
	int32 NumElements;
	
	UPROPERTY()
	bool bDensityCompressed;
	
private:
	FByteBulkData CompressedVoxelData;
	DA::TChunkDensityData<FDAVoxelData> VoxelData;
};


