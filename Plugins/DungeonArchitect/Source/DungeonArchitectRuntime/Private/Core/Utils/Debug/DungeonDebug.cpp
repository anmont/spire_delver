//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Utils/Debug/DungeonDebug.h"

#include "Core/Utils/EditorService/IDungeonEditorService.h"
#include "Frameworks/Lib/Voronoi/Voronoi.h"

#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogDungeonDebug);

ADungeonDebug::ADungeonDebug(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void ADungeonDebug::ExecuteDebugCommand(int32 CommandID) {
    switch (CommandID) {
    case 0: ExecuteCommand0();
        break;
    case 1: ExecuteCommand1();
        break;
    case 2: ExecuteCommand2();
        break;
    case 3: ExecuteCommand3();
        break;
    case 4: ExecuteCommand4();
        break;
    case 5: ExecuteCommand5();
        break;

    }
}

void ADungeonDebug::ExecuteCommand0() {
    TSharedPtr<IDungeonEditorService> Service = IDungeonEditorService::Get();
    Service->SaveDirtyPackages();
}

void ADungeonDebug::ExecuteCommand5() {

}

//////////////////////// ADungeonVoronoiTest ////////////////////////
ADungeonVoronoiTest::ADungeonVoronoiTest() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ADungeonVoronoiTest::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);

    //DrawVoronoiGraph();
    FDrawSettings DrawSettings;
    DrawSettings.bDrawEdges = true;
    DrawSettings.bDrawSites = true;
    DrawSettings.bDrawVertices = false;
    DrawSettings.bDrawDelaunay = true;
    DrawDCELGraph(DrawSettings);
}

void ADungeonVoronoiTest::BuildVoronoi() {
    using namespace DA;
    
    Sites.Reset();
    const FRandomStream Random(Seed);
    for (int i = 0; i < NumSeeds; i++) {
        Sites.Add({Random.FRand() * WorldSize, Random.FRand() * WorldSize});
    }

    auto GenerateVoronoiGraph = [this]() {
        FVoronoiGenerator::FSettings Settings;
        Settings.UnboundedEdgeExtension = WorldSize * 2;
        FVoronoiGenerator::Generate(VoronoiGraph, Sites, Settings);
    
        DGraph.Generate(VoronoiGraph);
    
        DCELGraphClipper Clipper;
        Clipper.ClipBoundary(DGraph, {0, 0}, {WorldSize, WorldSize});
    };
    
    GenerateVoronoiGraph();
    for (int i = 0; i < NumRelaxIterations; i++) {
        DGraph.RelaxPoints(Sites);
        GenerateVoronoiGraph();
    }
}

void ADungeonVoronoiTest::DrawVoronoiGraph() {
    using namespace DA;
    
    const UWorld* World = GetWorld();
    const FColor SiteColor = FColor::Green;
    const FColor LineColor = FColor::White;

    for (const FVector2d& Site : Sites) {
        DrawDebugPoint(World, FVector(Site.X, Site.Y, 0), 10.0f, SiteColor);
    }

    const TVorDataPool<FVorEdge>& Edges = VoronoiGraph.GetEdges();
    const TVorDataPool<FVorVertex>& Vertices = VoronoiGraph.GetVertices();
    for (int i = 0; i < Edges.GetSize(); i++) {
        const FVorEdge* Edge = Edges.Get(i);
        const FVector2d Start2D = Edge->Origin->Location;
        const FVector2d End2D = Edge->Twin->Origin->Location;
        const FVector Start{Start2D.X, Start2D.Y, 0};
        const FVector End{End2D.X, End2D.Y, 0};
        
        DrawDebugLine(World, Start, End, LineColor);
    }
}

void ADungeonVoronoiTest::DrawDCELGraph(const FDrawSettings& InSettings) {
    using namespace DA;
    
    const UWorld* World = GetWorld();
    const FColor SiteColor = FColor::Green;
    const FColor LineColor = FColor::White;

    if (InSettings.bDrawSites) {
        for (const FVector2d& Site : Sites) {
            DrawDebugPoint(World, FVector(Site.X, Site.Y, 0), 2, SiteColor);
        }
    }

    if (InSettings.bDrawEdges) {
        const TArray<DCEL::FEdge*>& Edges = DGraph.GetEdges();
        TSet<const DCEL::FEdge*> Visited;
        for (const DCEL::FEdge* Edge : Edges) {
            if (!Edge->bValid) continue;
            if (Visited.Contains(Edge)) continue;
            Visited.Add(Edge->Twin);
        
            const FVector2d Start2D = Edge->Origin->Location;
            const FVector2d End2D = Edge->Twin->Origin->Location;
            const FVector Start{Start2D.X, Start2D.Y, 0};
            const FVector End{End2D.X, End2D.Y, 0};
        
            DrawDebugLine(World, Start, End, LineColor);
        }
    }
    
    if (InSettings.bDrawVertices) {
        const TArray<DCEL::FVertex*>& Vertices = DGraph.GetVertices();
        TSet<FVector2d> Visited;
        for (const DCEL::FVertex* Vertex : Vertices) {
            if (!Vertex->bValid) continue;

            FColor Color = FColor::Red;
            const FVector2d Location = Vertex->Location;
            float LocZ = 0;
            float PointSize = 5;
            
            if (Visited.Contains(Location)) {
                Color = FColor::Blue;
                LocZ = 20;
                PointSize = 10;
            }
            Visited.Add(Location);
            
            DrawDebugPoint(World, FVector(Location.X, Location.Y, LocZ), PointSize, Color);
        }
    }

    if (InSettings.bDrawDelaunay) {
        const TArray<DCEL::FFace*>& Faces = DGraph.GetFaces();
        const TArray<DCEL::FEdge*>& Edges = DGraph.GetEdges();

        for (const DCEL::FEdge* Edge : Edges) {
            if (!Edge->bValid) continue;

            DCEL::FFace* Face0 = Edge->LeftFace;
            DCEL::FFace* Face1 = Edge->Twin->LeftFace;
            if (Face0 && Face1) {
                const FVector2d& Start2D = Sites[Face0->FaceId];
                const FVector2d& End2D = Sites[Face1->FaceId];
                const FVector Start{Start2D.X, Start2D.Y, 0};
                const FVector End{End2D.X, End2D.Y, 0};
                
                DrawDebugLine(World, Start, End, {64, 64, 64});
            }
        }
    }
    
}

