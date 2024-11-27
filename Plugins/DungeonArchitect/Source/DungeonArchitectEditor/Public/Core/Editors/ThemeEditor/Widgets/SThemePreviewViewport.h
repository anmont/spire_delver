//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Utils/DungeonEditorViewportProperties.h"
#include "Frameworks/ThemeEngine/DungeonThemeAsset.h"

#include "SEditorViewport.h"

class FThemeGraphAppMode;
class FDungeonArchitectThemeEditor;
class AThemeEditorMarkerVisualizer;

/**
* StaticMesh Editor Preview viewport widget
*/
class DUNGEONARCHITECTEDITOR_API SThemePreviewViewport : public SEditorViewport, public FGCObject,
                                                          public IDungeonEditorViewportPropertiesListener {
public:
    SLATE_BEGIN_ARGS(SThemePreviewViewport) {}
        SLATE_ARGUMENT(TWeakPtr<FDungeonArchitectThemeEditor>, DungeonEditor)
        SLATE_ARGUMENT(UDungeonThemeAsset*, ObjectToEdit)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    ~SThemePreviewViewport();

    // FGCObject interface
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
    virtual FString GetReferencerName() const override;
    // End of FGCObject interface

    /** Set the parent tab of the viewport for determining visibility */
    void SetParentTab(const FName& InAppMode, TSharedRef<SDockTab> InParentTab);

    void RebuildMeshes();

    // SWidget Interface
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
    // End of SWidget Interface

    EVisibility GetToolbarVisibility() const;

    void VisualizeMarkers(const TSet<FName>& InMarkerNames) const;
    void ClearMarkerVisualizations() const;

    virtual void OnPropertyChanged(FString PropertyName, UDungeonEditorViewportProperties* Properties) override;

    /* The nodes that were modified and requires a clean rebuild by the scene provider */
    void SetNodesToRebuild(const TSet<FName>& NodeIds);

    TSharedPtr<class FAdvancedPreviewScene> GetAdvancedPreview() const { return PreviewScene; }

    void ZoomOnNearestNodeMesh(const FName& InNodeId) const;
    
protected:
    /** SEditorViewport interface */
    virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
    virtual EVisibility OnGetViewportContentVisibility() const override;
    virtual void BindCommands() override;
    virtual void OnFocusViewportToSelection() override;
    virtual TSharedPtr<SWidget> MakeViewportToolbar() override;

    void PerformMeshRebuild();

    void OnShowPropertyDungeon() const;
    void OnShowPropertySkylight() const;
    void OnShowPropertyDirectionalLight() const;
    void OnShowPropertyAtmosphericFog() const;
    void OnToggleDebugData();

    void HandleForceRebuildPreviewLayout();
    bool HandleForceRebuildPreviewLayoutIsChecked() const;
    bool HandleToggleDebugDataIsChecked() const;

    void ShowObjectProperties(UObject* Object, bool bForceRefresh = false) const;
    void ListenToConfigChanges(UDungeonConfig* Config) const;

    void CreateDungeonBuilder(TSubclassOf<UDungeonBuilder> BuilderClass);

    void DestroyDungeonActors();
    void CleanupModifiedNodeObjects();

private:
    /** Determines the visibility of the viewport. */
    virtual bool IsVisible() const override;
    UDungeonModel* CreateModelInstance(UObject* Outer) const;
    UDungeonConfig* CreateConfigInstance(UObject* Outer) const;
    UDungeonQuery* CreateQueryInstance(UObject* Outer) const;
    void InitializeCamera(const UWorld* InWorld) const;

    
private:
    TWeakPtr<FDungeonArchitectThemeEditor> ThemeEditorPtr;
    TObjectPtr<UDungeonThemeAsset> ObjectToEdit = nullptr;

    // The parent tab where this viewport resides, mapped to the app mode id
    TMap<FName, TWeakPtr<SDockTab>> ParentTabs;

    /** Level viewport client */
    TSharedPtr<class FDungeonEditorViewportClient> EditorViewportClient;

    /** The scene for this viewport. */
    TSharedPtr<class FAdvancedPreviewScene> PreviewScene;

    TObjectPtr<UDungeonBuilder> DungeonBuilder = nullptr;
    TSharedPtr<class FDungeonSceneProvider> SceneProvider;

    bool bRequestRebuildPreviewMesh = false;

    bool bDrawDebugData = false;
    bool bForcePreviewRebuildLayout = false;

    TObjectPtr<USkyLightComponent> Skylight = nullptr;
    TObjectPtr<USkyAtmosphereComponent> AtmosphericFog = nullptr;

    // The nodes that were modified and requires a clean rebuild by the scene provider
    TSet<FName> NodeObjectsToRebuild;

    TObjectPtr<UDungeonModel> DungeonModel = nullptr;
    TObjectPtr<UDungeonQuery> DungeonQuery = nullptr;

    TObjectPtr<AThemeEditorMarkerVisualizer> Visualizer = nullptr;
};

