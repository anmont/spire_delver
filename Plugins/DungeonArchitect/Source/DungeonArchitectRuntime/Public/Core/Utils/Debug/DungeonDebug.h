//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Frameworks/Lib/Geometry/DCEL.h"
#include "Frameworks/Lib/Voronoi/VoronoiBeachLine.h"

#include "GameFramework/Actor.h"
#include "DungeonDebug.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDungeonDebug, Log, All);

UCLASS(Experimental, NotBlueprintable, hidecategories = (Rendering, Input, Actor, Misc))
class DUNGEONARCHITECTRUNTIME_API ADungeonDebug : public AActor {
    GENERATED_BODY()

public:
    ADungeonDebug(const FObjectInitializer& ObjectInitializer);

    void ExecuteDebugCommand(int32 CommandID);

    void ExecuteCommand0();

    void ExecuteCommand1() {
    }

    void ExecuteCommand2() {
    }

    void ExecuteCommand3() {
    }

    void ExecuteCommand4() {
    }

    void ExecuteCommand5();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
    AActor* CloneTemplate;
};


UCLASS()
class DUNGEONARCHITECTRUNTIME_API ADungeonVoronoiTest : public AActor {
    GENERATED_BODY()
public:
    ADungeonVoronoiTest();
    virtual void Tick(float DeltaSeconds) override;
    virtual bool ShouldTickIfViewportsOnly() const override { return true; }
    
    UFUNCTION(CallInEditor, Category="Voronoi")
    void BuildVoronoi();

public:
    UPROPERTY(EditAnywhere, Category="Voronoi")
    int Seed{};
    
    UPROPERTY(EditAnywhere, Category="Voronoi")
    int NumSeeds{15};
    
    UPROPERTY(EditAnywhere, Category="Voronoi")
    float WorldSize{6000};
    
    UPROPERTY(EditAnywhere, Category="Voronoi")
    int NumRelaxIterations{5};
    
private:
    void DrawVoronoiGraph();

    struct FDrawSettings {
        bool bDrawEdges{};
        bool bDrawSites{};
        bool bDrawVertices{};
        bool bDrawDelaunay{};
    };
    void DrawDCELGraph(const FDrawSettings& InSettings);
    
private:
    TArray<FVector2d> Sites;
    DA::FVoronoiGraph VoronoiGraph;
    DA::DCELGraph DGraph;
};

