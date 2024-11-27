//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Voxel/Scene/DungeonVoxelWorld.h"

#include "Core/Utils/MathUtils.h"
#include "Frameworks/Voxel/Meshing/Blocky/BlockMeshTools.h"

#include "Components/DynamicMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"

DEFINE_LOG_CATEGORY_STATIC(LogVDB, Log, All);

ADungeonVoxelWorld::ADungeonVoxelWorld() {
	MeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>("Mesh");
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	MeshComponent->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));		// is this necessary?
	
	SetRootComponent(MeshComponent);
}

void ADungeonVoxelWorld::BuildWorld() {
	using namespace UE::Geometry;
	using FLeafNode = DAVDB::FNodeLOD0;

	GenerateDensity();
	GenerateMesh();
}

void ADungeonVoxelWorld::RemoveBlock(const FVector& InLocation, const FVector& InNormal, float Radius) {
	const float GridRadius = Radius / VoxelSize;
	
	const FVector LocalLocation = GetActorTransform().InverseTransformPosition(InLocation);
	const FVector LocalNormal = GetActorTransform().InverseTransformVector(InNormal);
	const FVector Offset = (-VoxelSize * 0.5f) * LocalNormal;
	const FVector VoxelLocation = LocalLocation + Offset;
	const FIntVector GridCoord = FMathUtils::FloorToInt(VoxelLocation / VoxelSize);
	const int32 Extent = FMath::Floor(GridRadius);
	
	if (Extent == 0) {
		VoxelGrid.Set(GridCoord, { 0 });
	}
	else {
		const FVector Origin = FMathUtils::ToVector(GridCoord);
		const float GridRadiusSq = GridRadius * GridRadius;
		
		for (int x = -Extent; x <= Extent; x++) {
			for (int y = -Extent; y <= Extent; y++) {
				for (int z = -Extent; z <= Extent; z++) {
					FVector LocalCoord = FVector(x, y, z);
					const float DistSq = LocalCoord.SizeSquared();
					if (DistSq < GridRadiusSq) {
						bool bShouldClear = true;
						VoxelGrid.Set(GridCoord + FIntVector(x, y, z), { 0 });
					}
				}
			}
		}
	}
	
	GenerateMesh();
}

void ADungeonVoxelWorld::GenerateDensity() {
	VoxelGrid.Clear();
	
	const FRandomStream Random(Seed);
	// Build the density
	for (int x = CoordMin.X; x <= CoordMax.X; x++) {
		for (int y = CoordMin.Y; y <= CoordMax.Y; y++) {
			for (int z = CoordMin.Z; z <= CoordMax.Z; z++) {
				const bool bEmpty = Random.FRand() < 0.05f ? 1 : 0;
				if (!bEmpty) {
					DAVDB::FVoxelData VoxelData;
					VoxelData.Material = 1;
					VoxelGrid.Set(FIntVector(x, y, z), VoxelData);
				}
			}
		}
	}
}

void ADungeonVoxelWorld::GenerateMesh() {
	MeshComponent->EditMesh([this](FDynamicMesh3& EditMesh) {
		FDAVoxelMeshGenSettings Settings;
		Settings.VoxelSize = VoxelSize;
		Settings.CoordMin = CoordMin;
		Settings.CoordMax = CoordMax;
		Settings.bOptimizeMesh = bOptimizeMesh;
		Settings.Seed = Seed;

		FDAVoxelMeshGenerator::GenerateBlockMesh(EditMesh, VoxelGrid, Settings);
	});

	MeshComponent->UpdateCollision(false);
}


