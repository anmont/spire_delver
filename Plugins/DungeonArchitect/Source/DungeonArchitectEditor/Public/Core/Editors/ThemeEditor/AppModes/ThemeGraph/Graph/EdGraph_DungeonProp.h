//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/DungeonProp.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph_DungeonProp.generated.h"

class UDungeonThemeAsset;

struct FDungeonDataTypes {
    static const FName PinType_Mesh;
    static const FName PinType_Marker;
};

struct FDungeonGraphBuildError {
    class UEdGraphNode_DungeonBase* NodeWithError;
    FString ErrorMessage;
    FString ErrorTooltip;
};

UCLASS()
class UEdGraph_DungeonProp : public UEdGraph {
    GENERATED_UCLASS_BODY()

public:
    void RebuildGraph(UObject* Owner, TArray<FPropTypeData>& OutProps, TArray<FDungeonGraphBuildError>& OutErrors) const;

    void RecreateDefaultMarkerNodes(TSubclassOf<class UDungeonBuilder> BuilderClass);
    void RecreateDefaultMarkerNodes(const TArray<FString> InMarkerNames);

    /** Add a listener for OnGraphChanged events */
    FDelegateHandle AddOnNodePropertyChangedHandler(const FOnGraphChanged::FDelegate& InHandler);

    /** Remove a listener for OnGraphChanged events */
    void RemoveOnNodePropertyChangedHandler(FDelegateHandle Handle);

    void NotifyNodePropertyChanged(const FEdGraphEditAction& InAction);

    bool ContainsFreeSpace(int32 X, int32 Y, float Distance);

    bool IsAssetAcceptableForDrop(const UObject* AssetObject) const;

    template <typename TNodeClass>
    TNodeClass* CreateNewNode(const FVector2D& Location) {
        TNodeClass* Node = NewObject<TNodeClass>(this);
        Node->Rename(NULL, this, REN_NonTransactional);
        this->AddNode(Node, true, false);

        Node->CreateNewGuid();
        Node->PostPlacedNewNode();
        Node->AllocateDefaultPins();

        Node->NodePosX = Location.X;
        Node->NodePosY = Location.Y;
        Node->SnapToGrid(SNAP_GRID);

        return Node;
    }

    /**
     * Create a new node when an asset has been dropped into the theme editor.   Pass in the preview viewport's level to support asset drag drop
     * @param AssetObject The asset object that was dragged in to create a new node
     * @param InLevel The intermediate level used to spawn the template actor.  Pass in the preview viewport level
     * @param Location The Location of the node
     * @return The create Node
     */
    UEdGraphNode_DungeonBase* CreateNewNode(UObject* AssetObject, ULevel* InLevel, const FVector2D& Location);

private:
    /** A delegate that broadcasts a notification whenever the graph has changed. */
    FOnGraphChanged OnNodePropertyChanged;

    static const int32 SNAP_GRID;
};

