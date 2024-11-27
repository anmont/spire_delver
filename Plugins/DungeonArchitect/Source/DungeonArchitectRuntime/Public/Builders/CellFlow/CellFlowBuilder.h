//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/DungeonBuilder.h"
#include "Core/Utils/Attributes.h"
#include "Frameworks/Flow/FlowProcessor.h"
#include "CellFlowBuilder.generated.h"

class UCellFlowConfig;
class UCellFlowModel;
class UCellFlowQuery;

UCLASS(EarlyAccessPreview, Meta=(DisplayName="Cell Flow", Description="Build interesting level layout on a Grid or a Voronoi space with stair support.   It works by merging nearby cells into larger chunks and using the flow framework on top of it"))
class DUNGEONARCHITECTRUNTIME_API UCellFlowBuilder : public UDungeonBuilder {
	GENERATED_BODY()
public:
	virtual void BuildDungeonImpl(UWorld* World) override;
    virtual void DestroyDungeonImpl(UWorld* InWorld) override;
	virtual void EmitDungeonMarkers_Implementation() override;
	virtual bool SupportsBackgroundTask() const override { return false; }
	virtual TSubclassOf<UDungeonModel> GetModelClass() override;
	virtual TSubclassOf<UDungeonConfig> GetConfigClass() override;
	virtual TSubclassOf<UDungeonToolData> GetToolDataClass() override;
	virtual TSubclassOf<UDungeonQuery> GetQueryClass() override;
	
	virtual void GetDefaultMarkerNames(TArray<FString>& OutMarkerNames) override;
	virtual bool PerformSelectionLogic(const TArray<UDungeonSelectorLogic*>& SelectionLogics, const FDAMarkerInfo& socket) override;
	virtual FTransform PerformTransformLogic(const TArray<UDungeonTransformLogic*>& TransformLogics, const FDAMarkerInfo& socket) override;
	
protected:
	virtual TSharedPtr<IMarkerGenProcessor> CreateMarkerGenProcessor(const FTransform& InDungeonTransform) const override;

private:
	bool ExecuteGraph();
	void CreateDebugVisualizations(const FGuid& InDungeonId, const FTransform& InTransform) const;
	void DestroyDebugVisualizations(const FGuid& InDungeonId) const;
	void EmitCellMarker(const FString& InMarkerName, const FTransform& InDungeonTransform, const FTransform& InLocalTransform);
	void EmitGridMarkers();
	void EmitDcelMarkers();
	
	bool GetDungeonBounds(FIntVector& OutMin, FIntVector& OutMax) const;
	
	virtual bool EmitProceduralMarkers(const UProceduralMarkerEmitter* InProceduralMarkerEmitter) override;
	void GenerateBaseMesh(UWorld* InWorld, const FGuid& InDungeonId, const FTransform& InTransform) const;
	void DestroyBaseMesh(UWorld* InWorld, const FGuid& InDungeonId) const;

private:
	TWeakObjectPtr<UCellFlowModel> CellModel;
	TWeakObjectPtr<UCellFlowConfig> CellConfig;
    TWeakObjectPtr<UCellFlowQuery> CellQuery;
    FDAAttributeList AttributeList;
};


class DUNGEONARCHITECTRUNTIME_API FCellFlowProcessDomainExtender : public IFlowProcessDomainExtender {
public:
	virtual void ExtendDomains(FFlowProcessor& InProcessor) override;

};

