//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Frameworks/Voxel/VDB/VDBLib.h"

#include "GameFramework/Actor.h"
#include "DungeonVoxelWorld.generated.h"

class UDynamicMeshComponent;
namespace UE {
	namespace Geometry {
		class FDynamicMesh3;
	}
}

UCLASS()
class ADungeonVoxelWorld : public AActor {
	GENERATED_BODY()
public:
	ADungeonVoxelWorld();
	
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Voxel")
	void BuildWorld();

	UFUNCTION(BlueprintCallable, Category="Voxel")
	void RemoveBlock(const FVector& Location, const FVector& Normal, float Radius);
	
private:
	void GenerateDensity();
	void GenerateMesh();
	
private:
	DAVDB::FVoxelGrid VoxelGrid;

	UPROPERTY(EditAnywhere, Category=Voxel)
	UDynamicMeshComponent* MeshComponent;
	
	UPROPERTY(EditAnywhere, Category=Voxel)
	double VoxelSize = 12.5;
	
	UPROPERTY(EditAnywhere, Category=Voxel)
	int32 Seed = 0;
	
	UPROPERTY(EditAnywhere, Category=Voxel)
	bool bOptimizeMesh = true;

	UPROPERTY(EditAnywhere, Category=Voxel)
	FIntVector CoordMin = FIntVector(0, 0, 0);
	
	UPROPERTY(EditAnywhere, Category=Voxel)
	FIntVector CoordMax = FIntVector(15, 15, 15);
};

