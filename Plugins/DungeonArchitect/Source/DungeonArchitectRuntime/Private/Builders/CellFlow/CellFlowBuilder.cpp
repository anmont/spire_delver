//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Builders/CellFlow/CellFlowBuilder.h"

#include "Builders/CellFlow/CellFlowAsset.h"
#include "Builders/CellFlow/CellFlowConfig.h"
#include "Builders/CellFlow/CellFlowModel.h"
#include "Builders/CellFlow/CellFlowQuery.h"
#include "Builders/CellFlow/CellFlowSelectorLogic.h"
#include "Builders/CellFlow/CellFlowToolData.h"
#include "Builders/CellFlow/CellFlowTransformLogic.h"
#include "Core/Dungeon.h"
#include "Core/DungeonMarkerNames.h"
#include "Core/Utils/DungeonUtils.h"
#include "Core/Volumes/DungeonNegationVolume.h"
#include "Frameworks/Flow/Domains/LayoutGraph/Utils/FlowLayoutGraphVisualization.h"
#include "Frameworks/Flow/ExecGraph/FlowExecGraphScript.h"
#include "Frameworks/Flow/FlowProcessor.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/CellFlowLayoutGraph.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/CellFlowLayoutGraphDomain.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/CellFlowLayoutVisualization.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/Impl/CellFlowGrid.h"
#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/Tasks/CellFlowLayoutTaskCreateCellsVoronoi.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowLibGrid.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowLibVoronoi.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/CellFlowUtils.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/Meshing/CellFlowChunkMeshActor.h"
#include "Frameworks/FlowImpl/CellFlow/Lib/Meshing/MeshGeneratorDCEL.h"
#include "Frameworks/MarkerGenerator/Impl/Grid/MarkerGenGridProcessor.h"
#include "Frameworks/MarkerGenerator/MarkerGenLayer.h"
#include "Frameworks/MarkerGenerator/MarkerGenModel.h"
#include "Frameworks/Meshing/Geometry/DungeonProceduralMesh.h"
#include "Frameworks/ThemeEngine/Markers/ProceduralMarkers/ProceduralMarkerEmitter.h"

#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(CellFlowBuilderLog, Log, All);

void UCellFlowBuilder::BuildDungeonImpl(UWorld* World) {
	CellModel = Cast<UCellFlowModel>(DungeonModel);
	CellConfig = Cast<UCellFlowConfig>(DungeonConfig);
	CellQuery = Cast<UCellFlowQuery>(DungeonQuery);
	
	WorldMarkers.Reset();

	if (!CellModel.IsValid()) {
		UE_LOG(CellFlowBuilderLog, Error, TEXT("Invalid dungeon model provided to the cell flow builder"));
		return;
	}

	if (!CellConfig.IsValid()) {
		UE_LOG(CellFlowBuilderLog, Error, TEXT("Invalid dungeon config provided to the cell flow builder"));
		return;
	}

	if (const bool bGraphBuildSuccess = ExecuteGraph()) {
		const FGuid DungeonUid = Dungeon ? Dungeon->Uid : FGuid();
		const FTransform DungeonTransform = Dungeon ? Dungeon->GetActorTransform() : FTransform::Identity;
		DestroyBaseMesh(World, DungeonUid);
		GenerateBaseMesh(World, DungeonUid, DungeonTransform);
	
		if (Dungeon && Dungeon->bDrawDebugData) {
			CreateDebugVisualizations(DungeonUid, DungeonTransform);
		}
	}
	else {
		UE_LOG(CellFlowBuilderLog, Error, TEXT("Failed to execute cell flow graph"));
	}

}

void UCellFlowBuilder::DestroyDungeonImpl(UWorld* InWorld) {
	CellModel = Cast<UCellFlowModel>(DungeonModel);
	CellConfig = Cast<UCellFlowConfig>(DungeonConfig);
	CellQuery = Cast<UCellFlowQuery>(DungeonQuery);

	if (CellModel.IsValid()) {
		CellModel->Reset();
	}

	if (Dungeon) {
		DestroyBaseMesh(InWorld, Dungeon->Uid);
		DestroyDebugVisualizations(Dungeon->Uid);
	}
}

void UCellFlowBuilder::EmitDungeonMarkers_Implementation() {
	Super::EmitDungeonMarkers_Implementation();

	CellModel = Cast<UCellFlowModel>(DungeonModel);
	CellConfig = Cast<UCellFlowConfig>(DungeonConfig);
	CellQuery = Cast<UCellFlowQuery>(DungeonQuery);

	EmitGridMarkers();
	EmitDcelMarkers();
}

void UCellFlowBuilder::EmitGridMarkers() {
	if (!CellConfig.IsValid() || !CellModel.IsValid() || !CellModel->CellGraph || !CellModel->LayoutGraph) return;
	const FTransform DungeonTransform = Dungeon ? Dungeon->GetTransform() : FTransform::Identity;
	UDAFlowCellGraph* CellGraph = CellModel->CellGraph;
	const FFlowAbstractGraphQuery GraphQuery(CellModel->LayoutGraph);
	const FVector GridSize = CellConfig->GridSize;

	FCellFlowLibGrid::FCellFlowGridMarkerContext Context{ GridSize };
	
	if (UCellFlowConfigMarkerSettings* MarkerSettings = CellConfig->MarkerConfig.LoadSynchronous()) {
		FCellFlowUtils::GenerateGroupNodeMarkerSetup(MarkerSettings->DefaultGridPathMarkers, MarkerSettings->GridPathMarkers, CellGraph->GroupNodes, GraphQuery, Random, Context.GroupNodeChunkMarkers);
	}
	
	for (int GroupIdx = 0; GroupIdx < CellGraph->GroupNodes.Num(); GroupIdx++) {
		const FDAFlowCellGroupNode& GroupNode = CellGraph->GroupNodes[GroupIdx];
		if (!GroupNode.IsActive()) continue;

		const UFlowAbstractNode* LayoutNode = GraphQuery.GetNode(GroupNode.LayoutNodeID);
		if (!LayoutNode || !LayoutNode->bActive) {
			continue;
		}

		FCellFlowGridMarkerSetup ChunkMarkers{};
		if (const FCellFlowGridMarkerSetup* SearchPtr = Context.GroupNodeChunkMarkers.Find(GroupNode.GroupId)){
			ChunkMarkers = *SearchPtr;
		}
		for (const int LeafNodeId : GroupNode.LeafNodes) {
			if (const UDAFlowCellLeafNodeGrid* GridLeafNode = Cast<UDAFlowCellLeafNodeGrid>(CellGraph->LeafNodes[LeafNodeId])) {
				for (int dy = 0; dy < GridLeafNode->Size.Y; dy++) {
					for (int dx = 0; dx < GridLeafNode->Size.X; dx++) {
						FVector Coord(GridLeafNode->Location.X + dx + 0.5f, GridLeafNode->Location.Y + dy + 0.5f, GridLeafNode->LogicalZ);
						FVector WorldLoc = Coord * GridSize;
						EmitCellMarker(ChunkMarkers.GroundMarker, DungeonTransform, FTransform(WorldLoc));
					}
				}
			}
		}
	}
	
	FCellFlowLibGrid::FuncEmitGridCellMarker FnEmitMarker = [this, &DungeonTransform](const FString& MarkerName, const FTransform& MarkerTransform) {
		EmitCellMarker(MarkerName, DungeonTransform, MarkerTransform);
	};
	
	const TArray<FCellFlowGridEdgeInfo>& HalfEdges = CellGraph->GridInfo.HalfEdges;
	for (int i = 0; i < HalfEdges.Num(); i++) {
		const FCellFlowGridEdgeInfo& Edge = HalfEdges[i];

		// Check if the edge is invalid
		if (Edge.TileGroup == INDEX_NONE) {
			continue;
		}

		FCellFlowLibGrid::InsertEdgeMarkers(Context, Edge, CellGraph->GridInfo, FnEmitMarker);
	}

	for (const FDAFlowCellGraphSpawnInfo& SpawnInfo : CellGraph->GridInfo.SpawnInfo) {
		if (!SpawnInfo.Item.IsValid()) {
			continue;
		}
		const UFlowGraphItem* Item = SpawnInfo.Item.Get();
		const FVector Location = (SpawnInfo.Coord + FVector(0.5f, 0.5f, 0)) * GridSize;
		EmitCellMarker(Item->MarkerName, DungeonTransform, FTransform(Location));
	}

	// Run the Scatter Prop pattern generator
	if (CellGraph->ScatterSettings.Num() > 0) {
		const UDungeonThemeAsset* PatternThemeAsset = Cast<UDungeonThemeAsset>(StaticLoadObject(
			UDungeonThemeAsset::StaticClass(), nullptr, TEXT("/DungeonArchitect/Core/Builders/CellFlowContent/Patterns/_Internal_Theme_CellFlowPatterns")));

		auto GetLayerProbability = [](const UMarkerGenLayer* Layer, const FCellFlowLayoutTaskScatterPropSettings& Settings) {
			static const FName LayerTagBase = TEXT("Base");
			static const FName LayerTag1x1 = TEXT("1x1");
			static const FName LayerTag1x2 = TEXT("1x2");
			static const FName LayerTag1x3 = TEXT("1x3");
			if (Layer->Tags.Contains(LayerTagBase)) return 1.0f;
			if (Layer->Tags.Contains(LayerTag1x1)) return Settings.Prop1x1.bEnabled ? Settings.Prop1x1.Probability : 0.0f;
			if (Layer->Tags.Contains(LayerTag1x2)) return Settings.Prop1x2.bEnabled ? Settings.Prop1x2.Probability : 0.0f;
			if (Layer->Tags.Contains(LayerTag1x3)) return Settings.Prop1x3.bEnabled ? Settings.Prop1x3.Probability : 0.0f;
			return 0.0f;
		};
		
		TSharedPtr<IMarkerGenProcessor> MarkerGenerator = CreateMarkerGenProcessor(DungeonTransform);
		if (PatternThemeAsset && PatternThemeAsset->MarkerGenerationModel) {
			for (const FCellFlowLayoutTaskScatterPropSettings& ScatterSetting : CellGraph->ScatterSettings) {
				for (UMarkerGenLayer* MarkerGenLayer : PatternThemeAsset->MarkerGenerationModel->Layers) {
					const float LayerProbability = GetLayerProbability(MarkerGenLayer, ScatterSetting);
					MarkerGenLayer->Probability = LayerProbability;
					if (LayerProbability > 0) {
						TArray<FDAMarkerInfo> NewMarkers;
						FCellFlowLibGrid::TransformPatternLayer(MarkerGenLayer, ScatterSetting);
						if (MarkerGenerator->Process(MarkerGenLayer, WorldMarkers, Random, NewMarkers)) {
							WorldMarkers = NewMarkers;
						}
					}
				}
			}
		}
	}
}


void UCellFlowBuilder::EmitDcelMarkers() {
	if (!CellConfig.IsValid() || !CellModel.IsValid() || !CellModel->CellGraph || !CellModel->LayoutGraph) return;
	const FTransform DungeonTransform = Dungeon ? Dungeon->GetTransform() : FTransform::Identity;
	const UDAFlowCellGraph* CellGraph = CellModel->CellGraph;
	const UCellFlowLayoutGraph* LayoutGraph = CellModel->LayoutGraph;
	const UCellFlowVoronoiGraph* VoronoiData = CellModel->VoronoiData;
	const FVector GridSize = CellConfig->GridSize;
	const FFlowAbstractGraphQuery GraphQuery(CellModel->LayoutGraph);

	FCellFlowLibVoronoi::FEmitMarkersContext EmitMarkerContext;
	FCellFlowLibVoronoi::InitMarkerContext(CellModel->CellGraph, VoronoiData->DGraph, CellModel->LayoutGraph,
			CellConfig->MarkerConfig.LoadSynchronous(), GridSize, Random, EmitMarkerContext);

	// Draw the debug data
	UDASceneDebugDataComponent* SceneDebugData = Dungeon ? Dungeon->GetComponentByClass<UDASceneDebugDataComponent>() : nullptr;
	if (SceneDebugData) {
		SceneDebugData->RenderScale = CellConfig->GridSize;
		for (const auto& Entry : CellGraph->DCELInfo.Stairs) {
			int EdgeIdx = Entry.Key;
			const FCellFlowLibVoronoi::FStairGenInfo& StairGenInfo = EmitMarkerContext.StairGenInfoByEdge.FindOrAdd(EdgeIdx);
			SceneDebugData->Data.BoxEntries.Add({ StairGenInfo.OcclusionTransform, StairGenInfo.OcclusionBoxExtents, FColor::Cyan });
		}
	}

	//const int GlobalCliffDepth = FCellFlowLevelMeshLib::GetGlobalCliffDepth(LayoutGraph, CellGraph, EmitMarkerContext.LayoutNodes);
	for (const FDAFlowCellGroupNode& GroupNode : CellGraph->GroupNodes) {
		const UFlowAbstractNode** LayoutNodePtr = EmitMarkerContext.LayoutNodes.Find(GroupNode.LayoutNodeID);
		const UFlowAbstractNode* LayoutNode = LayoutNodePtr ? *LayoutNodePtr : nullptr;
		if (!LayoutNode || !LayoutNode->bActive) {
			continue;
		}
		const TArray<FVector2d>& Sites = VoronoiData->Sites;
		const DA::DCELGraph& DGraph = VoronoiData->DGraph;
		
		const TArray<DA::DCEL::FFace*>& Faces = DGraph.GetFaces();
		for (const int LeafNodeId : GroupNode.LeafNodes) {
			if (Faces.IsValidIndex(LeafNodeId)) {
				const UDAFlowCellLeafNode* LeafNode = CellGraph->LeafNodes[LeafNodeId];
				if (!LeafNode) continue;
				const int LogicalHeightZ = LeafNode->LogicalZ;
				
				const DA::DCEL::FFace* Face = Faces[LeafNodeId];
				if (!Face || !Face->bValid || !Face->Outer) continue;
				if (!Sites.IsValidIndex(LeafNodeId)) {
					continue;
				}

				FCellFlowLibVoronoi::TFuncEmitMarker FnEmitMarker = [this, SceneDebugData](const FCellFlowSizedMarkerDef& MarkerDef, const FTransform& MarkerTransform, const FTransform& OcclusionTransform) {
					EmitMarker(MarkerDef.MarkerName, MarkerTransform);
					if (SceneDebugData) {
						const float OcclusionDepth = MarkerDef.bOccludes ? MarkerDef.OcclusionDepth : 0;
						const FVector BoxExtent(MarkerDef.Size * 0.5f, OcclusionDepth * 0.5f, 0);
						SceneDebugData->Data.BoxEntries.Add({ OcclusionTransform, BoxExtent, FColor::Red });
					}
				};

				TSet<int> DoorEdgeIndices(CellGraph->DCELInfo.DoorEdges);
				DA::DCEL::TraverseFaceEdges(Face->Outer,
				[&EmitMarkerContext, &DoorEdgeIndices, &GroupNode, &FnEmitMarker](const DA::DCEL::FEdge* InEdge) {
					if (const bool bGroupBorderEdge = FCellFlowLibVoronoi::IsGroupBorderEdge(GroupNode, InEdge)) {
						if (DoorEdgeIndices.Contains(InEdge->Index)) {
							FCellFlowLibVoronoi::EmitDoorEdgeMarkers(EmitMarkerContext, InEdge, FnEmitMarker);
						}
						else {
							FCellFlowLibVoronoi::EmitEdgeMarkers(EmitMarkerContext, InEdge, FnEmitMarker);
						}
					}
				});
			}
		}
	}
	
	// Emit the spawned item markers
	for (const FDAFlowCellGraphSpawnInfo& SpawnInfo : CellGraph->DCELInfo.SpawnInfo) {
		if (!SpawnInfo.Item.IsValid()) {
			continue;
		}
		const UFlowGraphItem* Item = SpawnInfo.Item.Get();
		const FVector Location = SpawnInfo.Coord * GridSize;
		EmitCellMarker(Item->MarkerName, DungeonTransform, FTransform(Location));
	}
}

bool UCellFlowBuilder::EmitProceduralMarkers(const UProceduralMarkerEmitter* InProceduralMarkerEmitter) {
	if (!CellConfig.IsValid() || !CellModel.IsValid() || !CellModel->CellGraph || !CellModel->LayoutGraph) {
		return false;
	}
	
	const FTransform DungeonTransform = Dungeon ? Dungeon->GetTransform() : FTransform::Identity;
	const FVector GridSize = CellConfig->GridSize;
	
	if (const UBoxBoundaryMarkerEmitter* BoxBoundaryEmitter = Cast<UBoxBoundaryMarkerEmitter>(InProceduralMarkerEmitter)) {
		FIntVector CoordMin, CoordMax;
		if (!GetDungeonBounds(CoordMin, CoordMax)) {
			return false;
		}

		BoxBoundaryEmitter->EmitMarkers(GridSize, CoordMin, CoordMax, DungeonTransform, [this](const FString& InMarkerName, const FTransform& InTransform) {
			EmitMarker(InMarkerName, InTransform);
		});
	}

	return true;
}

void UCellFlowBuilder::GenerateBaseMesh(UWorld* InWorld, const FGuid& InDungeonId, const FTransform& InTransform) const {
	if (!InWorld || !CellConfig.IsValid()) {
		return;
	}

	if (CellConfig->bGenerateVoronoiBaseMesh) {
		int LODIndex = 0;
		int SubDivs = CellConfig->DefaultVoronoiMeshProfile.NumRenderSubDiv;
		const int CollisionSubDivs = CellConfig->DefaultVoronoiMeshProfile.NumCollisionSubDiv;
		const bool bApplyNoise = CellConfig->DefaultVoronoiMeshProfile.bApplyNoise;
		const bool bSmoothNormals = CellConfig->DefaultVoronoiMeshProfile.bSmoothNormals;
		const FCellFlowMeshNoiseSettings& NoiseSettings = CellConfig->DefaultVoronoiMeshProfile.NoiseSettings;
		

		struct FLODData {
			TArray<FFlowVisLib::FGeometry> SurfaceGeometries;
			bool bGenerateCollision{};
		};
		TArray<FLODData> LODList;
		LODList.SetNum(SubDivs + 1);

		while (SubDivs >= 0) {
			FLODData& LODInfo = LODList[LODIndex];
			FRandomStream MeshGenRandom(CellConfig->Seed);
			FCellFlowLevelMeshGenSettings RenderSettings;
			RenderSettings.VisualizationScale = CellConfig->GridSize;
			RenderSettings.NumSubDiv = SubDivs;
			RenderSettings.bSmoothNormals = bSmoothNormals;
			RenderSettings.Random = &MeshGenRandom;
			RenderSettings.bApplyNoise = bApplyNoise;
			RenderSettings.NoiseSettings = NoiseSettings;
			RenderSettings.bGeneratedMergedMesh = false;	// We want one mesh actor for each chunk
			#if WITH_EDITORONLY_DATA
			RenderSettings.bRenderInactive = CellModel->CellGraph ? CellModel->CellGraph->bRenderInactiveGroups : true;
#endif // WITH_EDITORONLY_DATA

			using namespace UE::Geometry;
			FCellFlowMeshGenDCEL::Generate(CellModel->LayoutGraph, CellModel->CellGraph, CellModel->VoronoiData, RenderSettings, LODInfo.SurfaceGeometries, nullptr);
			LODInfo.bGenerateCollision = (SubDivs == CollisionSubDivs);

			SubDivs--;
			LODIndex++;
		}

		// Spawn the chunk actors
		if (LODList.Num() > 0) {
			const int32 NumLODs = LODList.Num();
			const int32 NumChunks = LODList[0].SurfaceGeometries.Num();
			for (int SurfaceIdx = 0; SurfaceIdx < NumChunks; SurfaceIdx++) {
				// Bring the origin of the 
				ACellFlowChunkMesh* ChunkMesh = InWorld->SpawnActor<ACellFlowChunkMesh>();
				ChunkMesh->DungeonID = InDungeonId;

				// Calculate the offset
				FVector3d Offset;
				{
					FFlowVisLib::FGeometry& SurfaceGeometry = LODList[0].SurfaceGeometries[SurfaceIdx];
					FBox ChunkBounds = FFlowVisLib::GetBounds(SurfaceGeometry);
					Offset = ChunkBounds.GetCenter();
					Offset.Z = ChunkBounds.Max.Z;
					FTransform ChunkTransform = FTransform(Offset) * InTransform;
					ChunkMesh->SetActorTransform(ChunkTransform);
				}

			
				for (int LODIdx = 0; LODIdx < LODList.Num(); LODIdx++) {
					FLODData& LODInfo = LODList[LODIdx];
					FFlowVisLib::FGeometry& SurfaceGeometry = LODInfo.SurfaceGeometries[SurfaceIdx];
					FFlowVisLib::TranslateGeometry(-Offset, SurfaceGeometry);
					ChunkMesh->UploadGeometry(LODIdx, 0, SurfaceGeometry, LODInfo.bGenerateCollision);
				}
				
				// Set the material
				// TODO: Set the correct material, based on the chunk path id
				UMaterialInterface* Material = CellConfig->DefaultVoronoiMeshProfile.Material;
				ChunkMesh->GetMeshComponent()->SetMaterial(0, Material);
				ChunkMesh->GetMeshComponent()->LODFactorScale = CellConfig->DefaultVoronoiMeshProfile.LODFactorScale;

#if WITH_EDITOR
				if (ChunkMesh && Dungeon) {
					ChunkMesh->SetFolderPath(Dungeon->ItemFolderPath);
				}
#endif
			}
		}
	}
}

void UCellFlowBuilder::DestroyBaseMesh(UWorld* InWorld, const FGuid& InDungeonId) const {
	if (InWorld) {
		FDungeonUtils::DestroyManagedActor<ACellFlowChunkMesh>(InWorld, InDungeonId);
	}
}

bool UCellFlowBuilder::GetDungeonBounds(FIntVector& OutMin, FIntVector& OutMax) const {
	if (!CellConfig.IsValid() || !CellModel.IsValid() || !CellModel->CellGraph || !CellModel->LayoutGraph) {
		return false;
	}
	
	UDAFlowCellGraph* CellGraph = CellModel->CellGraph;
	const FFlowAbstractGraphQuery GraphQuery(CellModel->LayoutGraph);

	bool bBoundsInitialized{};
	
	for (const FDAFlowCellGroupNode& GroupNode : CellGraph->GroupNodes) {
		if (!GroupNode.IsActive()) continue;

		const UFlowAbstractNode* LayoutNode = GraphQuery.GetNode(GroupNode.LayoutNodeID);
		if (!LayoutNode || !LayoutNode->bActive) {
			continue;
		}
		
		for (const int LeafNodeId : GroupNode.LeafNodes) {
			if (const UDAFlowCellLeafNodeGrid* GridLeafNode = Cast<UDAFlowCellLeafNodeGrid>(CellGraph->LeafNodes[LeafNodeId])) {
				if (!bBoundsInitialized) {
					OutMin = FIntVector(GridLeafNode->Location.X, GridLeafNode->Location.Y, GridLeafNode->LogicalZ);
					OutMax = OutMin + FIntVector(GridLeafNode->Size.X, GridLeafNode->Size.Y, GridLeafNode->LogicalZ);
					bBoundsInitialized = true;
				}
				else {
					OutMin.X = FMath::Min(OutMin.X, GridLeafNode->Location.X);
					OutMin.Y = FMath::Min(OutMin.Y, GridLeafNode->Location.Y);
					OutMin.Z = FMath::Min(OutMin.Z, GridLeafNode->LogicalZ);

					OutMax.X = FMath::Max(OutMax.X, GridLeafNode->Location.X + GridLeafNode->Size.X);
					OutMax.Y = FMath::Max(OutMax.Y, GridLeafNode->Location.Y + GridLeafNode->Size.Y);
					OutMax.Z = FMath::Max(OutMax.Z, GridLeafNode->LogicalZ);
				}
			}
		}
	}
	return bBoundsInitialized;
}

void UCellFlowBuilder::CreateDebugVisualizations(const FGuid& InDungeonId, const FTransform& InTransform) const {
	DestroyDebugVisualizations(InDungeonId);

	if (!CellModel.IsValid() || !CellConfig.IsValid()) {
		return;
	}
	if (CellModel->LayoutGraph && CellModel->CellGraph) {
		UWorld* World = GetWorld();
		
		const float NodeRadius = FMath::Max(CellConfig->GridSize.X, CellConfig->GridSize.Y) * 0.4f;
		const FTransform BaseTransform = Dungeon ? Dungeon->GetActorTransform() : FTransform::Identity;
		const FTransform LayoutGraphOffsetZ = FTransform(FVector(0, 0, NodeRadius * 1.5f));
		
		AFlowLayoutGraphVisualizer* LayoutVisualizer = World->SpawnActor<AFlowLayoutGraphVisualizer>();
		LayoutVisualizer->DungeonID = InDungeonId;
		LayoutVisualizer->SetAutoAlignToLevelViewport(true);
		LayoutVisualizer->SetActorTransform(LayoutGraphOffsetZ * BaseTransform);

		FDAAbstractGraphVisualizerSettings VisualizerSettings;
		VisualizerSettings.NodeRadius = NodeRadius; // ModuleWidth * 0.05;
		VisualizerSettings.LinkThickness = VisualizerSettings.NodeRadius * 0.2f;
		VisualizerSettings.LinkRefThickness = VisualizerSettings.LinkThickness * 0.5f;
		VisualizerSettings.NodeSeparationDistance = CellConfig->GridSize;
		VisualizerSettings.DisabledNodeScale = 0.6f;
		VisualizerSettings.DisabledNodeOpacity = 0.75f;
		LayoutVisualizer->Generate(CellModel->LayoutGraph, VisualizerSettings);

		ACellFlowLayoutVisualization* CellVisualizer = World->SpawnActor<ACellFlowLayoutVisualization>();
		CellVisualizer->DungeonID = InDungeonId;
		CellVisualizer->SetActorTransform(BaseTransform);

		FCellFlowLevelMeshGenSettings RenderSettings;
		RenderSettings.bGeneratedMergedMesh = true;
		RenderSettings.bApplyNoise = false;
		RenderSettings.VisualizationScale = CellConfig->GridSize;
		
#if WITH_EDITORONLY_DATA
		RenderSettings.bRenderInactive = CellModel->CellGraph ? CellModel->CellGraph->bRenderInactiveGroups : true;
#endif // WITH_EDITORONLY_DATA
		
		CellVisualizer->Generate(CellModel->LayoutGraph, CellModel->CellGraph, CellModel->VoronoiData, RenderSettings);
	}
}

void UCellFlowBuilder::DestroyDebugVisualizations(const FGuid& InDungeonId) const {
	const UWorld* World = GetWorld();
	FDungeonUtils::DestroyManagedActor<AFlowLayoutGraphVisualizer>(World, InDungeonId);
	FDungeonUtils::DestroyManagedActor<ACellFlowLayoutVisualization>(World, InDungeonId);
}

void UCellFlowBuilder::EmitCellMarker(const FString& InMarkerName, const FTransform& InDungeonTransform, const FTransform& InLocalTransform) {
	EmitMarker(InMarkerName, InLocalTransform * InDungeonTransform);
}

TSubclassOf<UDungeonModel> UCellFlowBuilder::GetModelClass() {
	return UCellFlowModel::StaticClass();
}

TSubclassOf<UDungeonConfig> UCellFlowBuilder::GetConfigClass() {
	return UCellFlowConfig::StaticClass();
}

TSubclassOf<UDungeonToolData> UCellFlowBuilder::GetToolDataClass() {
	return UCellFlowToolData::StaticClass();
}

TSubclassOf<UDungeonQuery> UCellFlowBuilder::GetQueryClass() {
	return UCellFlowQuery::StaticClass();
}

void UCellFlowBuilder::GetDefaultMarkerNames(TArray<FString>& OutMarkerNames) {
	OutMarkerNames.Reset();
	OutMarkerNames.Add(FCellFlowBuilderMarkers::MARKER_GROUND);
	OutMarkerNames.Add(FCellFlowBuilderMarkers::MARKER_WALL);
	OutMarkerNames.Add(FCellFlowBuilderMarkers::MARKER_WALL_SEPARATOR);
	OutMarkerNames.Add(FCellFlowBuilderMarkers::MARKER_DOOR);
	OutMarkerNames.Add(FCellFlowBuilderMarkers::MARKER_DOOR_ONEWAY);
	OutMarkerNames.Add(FCellFlowBuilderMarkers::MARKER_FENCE);
	OutMarkerNames.Add(FCellFlowBuilderMarkers::MARKER_FENCE_SEPARATOR);
}

bool UCellFlowBuilder::PerformSelectionLogic(const TArray<UDungeonSelectorLogic*>& SelectionLogics, const FDAMarkerInfo& socket) {
	if (CellModel.IsValid() && CellConfig.IsValid() && CellQuery.IsValid()) {
		for (UDungeonSelectorLogic* SelectionLogic : SelectionLogics) {
			UCellFlowSelectorLogic* CellFlowSelectionLogic = Cast<UCellFlowSelectorLogic>(SelectionLogic);
			if (!CellFlowSelectionLogic) {
				UE_LOG(CellFlowBuilderLog, Warning,
					   TEXT("Invalid selection logic specified.  CellFlowSelectorLogic expected"));
				return false;
			}

			const bool bSelected = CellFlowSelectionLogic->SelectNode(CellModel.Get(), CellConfig.Get(), this, CellQuery.Get(), Random, socket.Transform);
			if (!bSelected) {
				return false;
			}
		}
	}
    
	return true;
}

FTransform UCellFlowBuilder::PerformTransformLogic(const TArray<UDungeonTransformLogic*>& TransformLogics, const FDAMarkerInfo& socket) {
	FTransform Result = FTransform::Identity;

	if (CellModel.IsValid() && CellConfig.IsValid() && CellQuery.IsValid()) {
		for (UDungeonTransformLogic* TransformLogic : TransformLogics) {
			UCellFlowTransformLogic* CellFlowTransformLogic = Cast<UCellFlowTransformLogic>(TransformLogic);
			if (!CellFlowTransformLogic) {
				UE_LOG(CellFlowBuilderLog, Warning,
					   TEXT("Invalid transform logic specified.  CellFlowTransformLogic expected"));
				continue;
			}

			FTransform LogicOffset;
			if (TransformLogic) {
				CellFlowTransformLogic->GetNodeOffset(CellModel.Get(), CellConfig.Get(), CellQuery.Get(), Random, socket.Transform, LogicOffset);
			}
			else {
				LogicOffset = FTransform::Identity;
			}

			FTransform Out;
			FTransform::Multiply(&Out, &LogicOffset, &Result);
			Result = Out;
		}
	}
    
	return Result;
}

TSharedPtr<IMarkerGenProcessor> UCellFlowBuilder::CreateMarkerGenProcessor(const FTransform& InDungeonTransform) const {
	if (const UCellFlowConfig* Config = CellConfig.IsValid() ? CellConfig.Get() : Cast<UCellFlowConfig>(DungeonConfig)) {
		const FVector GridSize = Config->GridSize;
		return MakeShareable(new FMarkerGenGridProcessor(InDungeonTransform, GridSize));
	}
	return nullptr;
}

bool UCellFlowBuilder::ExecuteGraph() {
	CellModel->LayoutGraph = nullptr;

	const UCellFlowAsset* CellFlowAsset = CellConfig->CellFlow.LoadSynchronous();

    if (!CellFlowAsset) {
        UE_LOG(CellFlowBuilderLog, Error, TEXT("Missing Cell Flow graph"));
        return false;
    }

    if (!CellFlowAsset->ExecScript) {
        UE_LOG(CellFlowBuilderLog, Error, TEXT("Invalid Cell Flow graph state. Please resave in editor"));
        return false;
    }

    FFlowProcessor FlowProcessor;
    
    // Register the domains
    {
        FCellFlowProcessDomainExtender Extender;
        Extender.ExtendDomains(FlowProcessor);
    }
	
    const int32 MAX_RETRIES = FMath::Max(1, CellConfig->MaxRetries);
    int32 NumTries = 0;
    FFlowProcessorResult Result;
    while (NumTries < MAX_RETRIES) {
        FFlowProcessorSettings FlowProcessorSettings;
        FlowProcessorSettings.AttributeList = AttributeList;
        FlowProcessorSettings.SerializedAttributeList = CellConfig->ParameterOverrides;
    	FlowProcessorSettings.Dungeon = Dungeon;
        Result = FlowProcessor.Process(CellFlowAsset->ExecScript, Random, FlowProcessorSettings);
        NumTries++;
        if (Result.ExecResult == EFlowTaskExecutionResult::Success) {
            break;
        }
        if (Result.ExecResult == EFlowTaskExecutionResult::FailHalt) {
            break;
        }
    }

    if (Result.ExecResult != EFlowTaskExecutionResult::Success) {
        UE_LOG(CellFlowBuilderLog, Error, TEXT("Failed to generate grid flow graph"));
        return false;
    }

    if (!CellFlowAsset->ExecScript->ResultNode) {
        UE_LOG(CellFlowBuilderLog, Error, TEXT("Cannot find result node in the grid flow exec graph. Please resave the grid flow asset in the editor"));
        return false;
    }

    const FGuid ResultNodeId = CellFlowAsset->ExecScript->ResultNode->NodeId;
    if (FlowProcessor.GetNodeExecStage(ResultNodeId) != EFlowTaskExecutionStage::Executed) {
        UE_LOG(CellFlowBuilderLog, Error, TEXT("Grid Flow Graph execution failed"));
        return false;
    }

    FFlowExecutionOutput ResultNodeState;
    FlowProcessor.GetNodeState(ResultNodeId, ResultNodeState);
    if (ResultNodeState.ExecutionResult != EFlowTaskExecutionResult::Success) {
        UE_LOG(CellFlowBuilderLog, Error, TEXT("Grid Flow Result node execution did not succeed"));
        return false;
    }

    // Save a copy in the model
    if (CellModel.IsValid()) {
        UCellFlowModel* CellModelPtr = CellModel.Get();

    	// Clone the layout graph and save it in the model
	    {
		    const UCellFlowLayoutGraph* ResultLayoutGraphState = ResultNodeState.State->GetState<UCellFlowLayoutGraph>(UFlowAbstractGraphBase::StateTypeID);
        	CellModel->LayoutGraph = NewObject<UCellFlowLayoutGraph>(CellModelPtr, "LayoutGraph");
        	if (ResultLayoutGraphState) {
        		CellModel->LayoutGraph->CloneFromStateObject(ResultLayoutGraphState);
        	}
	    }

    	// Clone the cell graph and save it in the model
        {
        	const UDAFlowCellGraph* ResultCellGraphState = ResultNodeState.State->GetState<UDAFlowCellGraph>(UDAFlowCellGraph::StateTypeID);
        	CellModel->CellGraph = NewObject<UDAFlowCellGraph>(CellModelPtr, "CellGraph");
        	if (ResultCellGraphState) {
        		CellModel->CellGraph->CloneFromStateObject(ResultCellGraphState);
        	}
        }

    	// Clone the Voronoi graph data
        {
        	const UCellFlowVoronoiGraph* ResultVoronoiData = ResultNodeState.State->GetState<UCellFlowVoronoiGraph>(UCellFlowVoronoiGraph::StateTypeID);
        	CellModel->VoronoiData = NewObject<UCellFlowVoronoiGraph>(CellModelPtr, "VoronoiData");
        	if (ResultVoronoiData) {
        		CellModel->VoronoiData->CloneFromStateObject(ResultVoronoiData);
        	}
        }
    	
    }
    return true;
}

void FCellFlowProcessDomainExtender::ExtendDomains(FFlowProcessor& InProcessor) {
	const TSharedPtr<FCellFlowLayoutGraphDomain> AbstractGraphDomain = MakeShareable(new FCellFlowLayoutGraphDomain);
	InProcessor.RegisterDomain(AbstractGraphDomain);
}

// My son's first line of code :) 02-Sep-23: 0.0t54f0-ok,;l5v,ṭxcccgbjk m  weffffffffzasssssssss3ce

