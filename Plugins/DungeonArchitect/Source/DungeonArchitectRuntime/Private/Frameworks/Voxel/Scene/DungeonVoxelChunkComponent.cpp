//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Voxel/Scene/DungeonVoxelChunkComponent.h"

#include "Frameworks/Voxel/Utils/VoxelLog.h"

void UDungeonVoxelChunkComponent::Serialize(FArchive& Ar) {
	Super::Serialize(Ar);
	
	CompressedVoxelData.Serialize(Ar, this, INDEX_NONE, false);
}

void UDungeonVoxelChunkComponent::Generate(const DA::FChunkDensityData& InData) {
	
}

void UDungeonVoxelChunkComponent::SaveData() {
	const TArray<FDAVoxelData>& RawData = VoxelData.GetData();
	const int32 OriginalSize = RawData.Num() * sizeof(FDAVoxelData);
		
	// It is possible for compression to actually increase the size of the data, so we over allocate here to handle that.
	int32 CompressedSize = OriginalSize * 4 / 3;
	TArray<uint8> CompressedData;
	CompressedData.SetNumUninitialized(CompressedSize);
		
	// Save the chunk data to the compressed archive
	if (FCompression::CompressMemory(NAME_Zlib, CompressedData.GetData(), CompressedSize, RawData.GetData(), OriginalSize, COMPRESS_BiasSpeed))
	{
		// In the case that compressing it actually increases the size, we leave it uncompressed
		if (CompressedSize < OriginalSize) {
			bDensityCompressed = true;
		}
	}

	const int32 SizeToSave = bDensityCompressed ? CompressedSize : OriginalSize;
	const void* BufferToSave = bDensityCompressed
			? static_cast<const void*>(CompressedData.GetData())
			: static_cast<const void*>(RawData.GetData());

	void* BulkBuffer = CompressedVoxelData.Lock(LOCK_READ_WRITE);
	if (CompressedVoxelData.GetBulkDataSize() != SizeToSave) {
		BulkBuffer = CompressedVoxelData.Realloc(SizeToSave);
	}

	FMemory::Memcpy(BulkBuffer, BufferToSave, SizeToSave);
	CompressedVoxelData.Unlock();
	NumElements = RawData.Num();
}

void UDungeonVoxelChunkComponent::LoadData() {
	// Load the compressed data from the archive to the chunk data
	TArray<FDAVoxelData>& RawData = VoxelData.GetData();
	RawData.SetNum(NumElements);
		
	const void* CompressedBuffer = CompressedVoxelData.LockReadOnly();
	const int32 CompressedSize = CompressedVoxelData.GetBulkDataSize();
		
	if (bDensityCompressed) {
		const int32 UncompressedSize = NumElements * sizeof(FDAVoxelData);
		if (!FCompression::UncompressMemory(NAME_Zlib, RawData.GetData(), UncompressedSize, CompressedBuffer, CompressedSize))
		{
			UE_LOG(LogDAVoxel, Error, TEXT("Failed to decompress chunk"));
			check(false);
		}
	}
	else {
		check(CompressedSize == sizeof(FDAVoxelData) * NumElements)
		FMemory::Memcpy(RawData.GetData(), CompressedBuffer, CompressedSize);
	}

	CompressedVoxelData.Unlock();
}

