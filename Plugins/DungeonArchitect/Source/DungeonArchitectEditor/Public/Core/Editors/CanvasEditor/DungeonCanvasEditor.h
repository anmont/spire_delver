//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditorSettings.h"
#include "Core/Utils/DungeonEditorViewportProperties.h"
#include "Frameworks/Canvas/DungeonCanvas.h"
#include "Frameworks/GraphGrammar/Editor/SEditableListView.h"

#include "BlueprintEditor.h"

class ADungeon;
class SDungeonCanvasEditorViewport;
class FDungeonCanvasBlueprintEditorToolbar;
class ADungeonCanvas;
class UDungeonCanvasBlueprint;

class DUNGEONARCHITECTEDITOR_API FDungeonCanvasEditor
	: public FBlueprintEditor
	, public IDungeonEditorViewportPropertiesListener
{
public:
	FDungeonCanvasEditor();
	void InitDungeonCanvasEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UDungeonCanvasBlueprint* InDungeonCanvasBlueprint);

	//~ Begin FBlueprintEditor Interface
	virtual void CreateDefaultCommands() override;
	virtual UBlueprint* GetBlueprintObj() const override;
	virtual FGraphAppearanceInfo GetGraphAppearance(UEdGraph* InGraph) const override;
	virtual bool IsInAScriptingMode() const override { return true; }
	virtual void SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents) override;
	virtual void RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated = false) override;
	virtual void PostUndo(bool bSuccessful) override;
	virtual void PostRedo(bool bSuccessful) override;
	virtual void Compile() override;
	//~ End FBlueprintEditor Interface

	//~ Begin IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetDocumentationLink() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;
	virtual void SaveAsset_Execute() override;
	//~ End IToolkit Interface

	//~ Begin FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject Interface

	
	//~ Begin IDungeonEditorViewportPropertiesListener Interface
	virtual void OnPropertyChanged(FString PropertyName, class UDungeonEditorViewportProperties* Properties) override;
	//~ End IDungeonEditorViewportPropertiesListener Interface
	
	/** Immediately rebuilds the dungeon canvas that is being shown in the editor. */
	void RefreshInstance(bool bForceFullUpdate);
	
	virtual TSharedPtr<FDungeonCanvasBlueprintEditorToolbar> GetDungeonCanvasToolbarBuilder() { return DungeonCanvasToolbar; }

	UDungeonCanvasBlueprint* GetDungeonCanvasBlueprint() const;
	ADungeonCanvas* GetInstance() const;
	TSharedPtr<SDungeonCanvasEditorViewport> GetCanvasViewport() const { return CanvasViewport; }
	TSharedPtr<SWidget> GetMaterialLayerListWidget() const { return MaterialLayerList; }
	void RecreateDungeonBuilder(UDungeonEditorViewportProperties* InViewportProperties);

	void CalculateTextureDimensions(int32& OutWidth, int32& OutHeight, int32& OutDepth, int32& OutArraySize, bool bInIncludeBorderSize) const;
	ESimpleElementBlendMode GetColourChannelBlendMode() const;

	EDungeonCanvasEditorSampling GetSampling() const;
	void SetZoomMode(const EDungeonCanvasEditorZoomMode ZoomMode);
	double GetCustomZoomLevel() const;
	void SetCustomZoomLevel(double ZoomValue);
	EDungeonCanvasEditorZoomMode GetZoomMode() const;
	double CalculateDisplayedZoomLevel() const;
	void ZoomIn();
	void ZoomOut();
	
private:
	/** Called whenever the blueprint is structurally changed. */
	virtual void OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled = false) override;

	const TArray<UDungeonCanvasMaterialLayer*>* GetMaterialLayerList() const;
	void OnMaterialLayerSelectionChanged(UDungeonCanvasMaterialLayer* Item, ESelectInfo::Type SelectInfo);
	void AddNewMaterialLayer(const FText& InLayerName, const FText& InLayerDescription, const TSoftObjectPtr<UMaterialFunctionMaterialLayer>& InMaterialLayer, const TSoftObjectPtr<UMaterialFunctionMaterialLayerBlend>& InMaterialBlend);
	void OnMaterialLayerDelete(UDungeonCanvasMaterialLayer* Item);
	void OnMaterialLayerReordered(UDungeonCanvasMaterialLayer* Source, UDungeonCanvasMaterialLayer* Dest);
	FText GetMaterialLayerListRowText(UDungeonCanvasMaterialLayer* Item) const;
	FText GetLayerListRowDescriptionText(UDungeonCanvasMaterialLayer* Item) const;
	EVisibility IsLayerListRowDescriptionVisible(UDungeonCanvasMaterialLayer* Item) const;
	TSharedPtr<SWidget> CreateMaterialLayerListItemWidget(UDungeonCanvasMaterialLayer* InMaterialLayer);

	void RecompileMaterialTemplate();
	void UpdateAssetThumbnail() const;

	TSharedRef<SWidget> GetAddMaterialLayerMenu();
	void RandomizePreviewDungeon() const;

protected:
	//~ Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~ End FGCObject Interface

	//~ Begin FNotifyHook Interface
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged ) override;
	//~ End FNotifyHook Interface

protected:
	/** Binds the FDungeonCanvasEditorCommands commands to functions in this editor. */
	void BindCommands();

	/** Undo the last action. */
	void UndoAction();

	/** Redo the last action that was undone. */
	void RedoAction();

	UObject* GetClassDefaultObject() const;
	void EditPreviewDungeonSettings_Clicked();
	bool IsDetailsPanelEditingDungeonPreviewSettings() const;

	DECLARE_MULTICAST_DELEGATE(FOnDungeonCanvasChanged);
	virtual FOnDungeonCanvasChanged& OnDungeonCanvasChanged() { return OnDungeonCanvasChangedDelegate; }
	
private:
	/** Extends the menu. */
	void ExtendMenu();

	/** Extends the toolbar. */
	void ExtendToolbar();

	/** Fills the toolbar with content. */
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);

	/** Destroy the render grid instance that is currently visible in the editor. */
	void DestroyInstance();

	/** Makes a newly compiled/opened render grid instance visible in the editor. */
	void UpdateInstance(UBlueprint* InBlueprint, bool bInForceFullUpdate);

	void BuildPreviewDungeon() const;
	//void RefreshMaterialListView() const;
	
	/** Wraps the normal blueprint editor's action menu creation callback. */
	FActionMenuContent HandleCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);
	void ShowPreviewDungeonSettings();

	void ShowDetailsForSingleObject(UObject* Object, const SKismetInspector::FShowDetailsOptions& Options = SKismetInspector::FShowDetailsOptions()) const;
	
private:
	/** The toolbar builder that is used to customize the toolbar of this editor. */
	TSharedPtr<FDungeonCanvasBlueprintEditorToolbar> DungeonCanvasToolbar;

protected:
	/** The extender to pass to the level editor to extend it's window menu. */
	TSharedPtr<FExtender> MenuExtender;

	/** The toolbar extender of this editor. */
	TSharedPtr<FExtender> ToolbarExtender;

	/** The blueprint instance that's currently visible in the editor. */
	TWeakObjectPtr<UDungeonCanvasBlueprint> PreviewBlueprintWeakPtr;

	/** The current render grid instance that's visible in the editor. */
	mutable TObjectPtr<ADungeonCanvas> CanvasInstance{};

private:
	/** The delegate for when data in the render grid changed. */
	FOnDungeonCanvasChanged OnDungeonCanvasChangedDelegate;

private:
	TWeakObjectPtr<ADungeon> PreviewDungeon;

	/** Viewport */
	TSharedPtr<SDungeonCanvasEditorViewport> CanvasViewport;

	TWeakObjectPtr<UDungeonCanvasMaterialLayer> ActiveMaterialLayer;
	TWeakObjectPtr<UDungeonCanvasEditorDefaults> EditorDefaultSettings;

	/* Material Layer List */
	TSharedPtr<SEditableListView<UDungeonCanvasMaterialLayer*>> MaterialLayerList;
	
	/** This toolkit's current sampling mode **/
	EDungeonCanvasEditorSampling Sampling;
	
	/** This toolkit's current zoom mode **/
	EDungeonCanvasEditorZoomMode ZoomMode;

	/** The maximum width/height at which the texture will render in the preview window */
	int32 PreviewTexWidth;
	int32 PreviewTexHeight;

	bool bRecompileNextFrame{};
	
	/** The texture's zoom factor. */
	double Zoom;

	class FCompileGuard {
	public:
		void Initialize(FDungeonCanvasEditor* InEditorPtr);
		void OnScopeStart(const FGuid& InScopeId);
		void OnScopeEnd(const FGuid& InScopeId);
		FORCEINLINE bool IsActive() const { return ActiveScopeId.IsValid(); }

	private:
		void SaveState();
		void RestoreState();

	private:
		FGuid ActiveScopeId;
		FDungeonCanvasEditor* EditorPtr{};
		TArray<FString> SelectedObjectPaths;
	};
	FCompileGuard CompileGuard;

	struct FCompileScope {
		FGuid ScopeId;
		FCompileGuard* CompileGuardPtr;
		FCompileScope(FCompileGuard* InCompileGuardPtr) {
			ScopeId = FGuid::NewGuid();
			CompileGuardPtr = InCompileGuardPtr;
			if (CompileGuardPtr) {
				CompileGuardPtr->OnScopeStart(ScopeId);
			}
		}
		~FCompileScope() {
			if (CompileGuardPtr) {
				CompileGuardPtr->OnScopeEnd(ScopeId);
			}
		}
	};
};

