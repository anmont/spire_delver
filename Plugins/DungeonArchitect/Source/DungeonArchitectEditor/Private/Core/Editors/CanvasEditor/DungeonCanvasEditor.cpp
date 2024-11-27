//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/CanvasEditor/DungeonCanvasEditor.h"

#include "Builders/CellFlow/CellFlowConfig.h"
#include "Builders/GridFlow/GridFlowConfig.h"
#include "Builders/SnapGridFlow/SnapGridFlowDungeon.h"
#include "Builders/SnapMap/SnapMapDungeonConfig.h"
#include "Core/Dungeon.h"
#include "Core/DungeonConfig.h"
#include "Core/Editors/CanvasEditor/AppModes/DungeonCanvasApplicationModeCanvasGraph.h"
#include "Core/Editors/CanvasEditor/AppModes/DungeonCanvasApplicationModes.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditorCommands.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditorConstants.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditorSettings.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditorToolbar.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditorUtilities.h"
#include "Core/Editors/CanvasEditor/Viewport/SDungeonCanvasEditorViewport.h"
#include "Frameworks/Canvas/DungeonCanvas.h"
#include "Frameworks/Canvas/DungeonCanvasBlueprint.h"
#include "Frameworks/Canvas/DungeonCanvasBlueprintGeneratedClass.h"

#include "AssetViewUtils.h"
#include "BlueprintCompilationManager.h"
#include "ContentBrowserModule.h"
#include "EditorModeManager.h"
#include "Framework/Commands/GenericCommands.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"
#include "SBlueprintEditorToolbar.h"
#include "Slate/SceneViewport.h"
#include "UObject/SoftObjectPtr.h"
#include "WorkflowOrientedApp/ApplicationMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogCanvasEditor, Log, All);

#define LOCTEXT_NAMESPACE "DungeonCanvasEditor"

namespace DA::DungeonCanvas::Private
{
	const FName DungeonCanvasEditorAppName(TEXT("DungeonCanvasEditorApp"));

}


FDungeonCanvasEditor::FDungeonCanvasEditor()
	: Sampling()
	, ZoomMode()
	, PreviewTexWidth(0)
	, PreviewTexHeight(0)
	, Zoom(0)
{
}

void FDungeonCanvasEditor::InitDungeonCanvasEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UDungeonCanvasBlueprint* InDungeonCanvasBlueprint) {
	
	check(InDungeonCanvasBlueprint);

	FBlueprintCompilationManager::FlushCompilationQueue(nullptr);
	CompileGuard.Initialize(this);

	TSharedPtr<FDungeonCanvasEditor> ThisPtr(SharedThis(this));
	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}

	// Build up a list of objects being edited in this asset editor
	TArray<UObject*> ObjectsBeingEdited;
	ObjectsBeingEdited.Add(InDungeonCanvasBlueprint);
	
	const UDungeonCanvasEditorSettings& Settings = *GetDefault<UDungeonCanvasEditorSettings>();
	Sampling = Settings.Sampling;
	ZoomMode = Settings.ZoomMode;
	Zoom = 1.0f;
	PreviewTexWidth = 1024;
	PreviewTexHeight = 1024;

	CanvasViewport = SNew(SDungeonCanvasEditorViewport, SharedThis(this))
		.OnDungeonRebuild_Raw(this, &FDungeonCanvasEditor::RandomizePreviewDungeon);
	
	BindCommands();
	
	// Initialize the asset editor and spawn tabs
	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, DA::DungeonCanvas::Private::DungeonCanvasEditorAppName,
			FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsBeingEdited);

	DungeonCanvasToolbar = MakeShareable(new FDungeonCanvasBlueprintEditorToolbar());
	
	CreateDefaultCommands();

	TArray<UBlueprint*> CanvasBlueprints;
	CanvasBlueprints.Add(InDungeonCanvasBlueprint);

	CommonInitialization(CanvasBlueprints, false);

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
	
	if (UDungeonCanvasBlueprint* CanvasBlueprint = GetDungeonCanvasBlueprint()) {
		if (CanvasBlueprint->PreviewDungeonProperties) {
			CanvasBlueprint->PreviewDungeonProperties->PropertyChangeListener = SharedThis(this);
		}

		CanvasBlueprint->MaterialLayers.RemoveAll([](const UDungeonCanvasMaterialLayer* Item) {
			return Item == nullptr;
		});
	}

	const FSoftObjectPath MaterialLayerPresetsPath(TEXT("/DungeonArchitect/Core/Editors/CanvasEditor/Data/DungeonCanvasEditorMaterialLayerPresets.DungeonCanvasEditorMaterialLayerPresets"));
	EditorDefaultSettings = Cast<UDungeonCanvasEditorDefaults>(MaterialLayerPresetsPath.TryLoad());
	
	MaterialLayerList = SNew(SEditableListView<UDungeonCanvasMaterialLayer*>)
		.GetListSource(this, &FDungeonCanvasEditor::GetMaterialLayerList)
		.OnSelectionChanged(this, &FDungeonCanvasEditor::OnMaterialLayerSelectionChanged)
		.OnDeleteItem(this, &FDungeonCanvasEditor::OnMaterialLayerDelete)
		.OnGetAddButtonMenuContent(this, &FDungeonCanvasEditor::GetAddMaterialLayerMenu)
		.OnReorderItem(this, &FDungeonCanvasEditor::OnMaterialLayerReordered)
		.GetItemText(this, &FDungeonCanvasEditor::GetMaterialLayerListRowText)
		.CreateItemWidget(this, &FDungeonCanvasEditor::CreateMaterialLayerListItemWidget)
		.IconBrush(FDungeonArchitectStyle::Get().GetBrush("DA.SnapEd.GraphIcon"))
		.AllowDropOnGraph(false)
	;

	UpdateInstance(GetDungeonCanvasBlueprint(), true);
	constexpr bool bShouldOpenInDefaultsMode = true;
	
	RegisterApplicationModes(CanvasBlueprints, bShouldOpenInDefaultsMode, InDungeonCanvasBlueprint->bIsNewlyCreated);

	// Post-layout initialization
	PostLayoutBlueprintEditorInitialization();

	if (GetBlueprintObj()->Status != BS_UpToDate) {
		Compile();
	}
}

void FDungeonCanvasEditor::CreateDefaultCommands() {
	if (GetBlueprintObj())
	{
		FBlueprintEditor::CreateDefaultCommands();
	}
	else
	{
		ToolkitCommands->MapAction(FGenericCommands::Get().Undo, FExecuteAction::CreateSP(this, &FDungeonCanvasEditor::UndoAction));
		ToolkitCommands->MapAction(FGenericCommands::Get().Redo, FExecuteAction::CreateSP(this, &FDungeonCanvasEditor::RedoAction));
	}
}

UBlueprint* FDungeonCanvasEditor::GetBlueprintObj() const {
	for (UObject* Obj : GetEditingObjects())
	{
		if (!IsValid(Obj))
		{
			continue;
		}
		if (UBlueprint* Result = Cast<UBlueprint>(Obj))
		{
			return Result;
		}
	}
	return nullptr;
}

FGraphAppearanceInfo FDungeonCanvasEditor::GetGraphAppearance(UEdGraph* InGraph) const {
	FGraphAppearanceInfo AppearanceInfo = FBlueprintEditor::GetGraphAppearance(InGraph);
	if (GetBlueprintObj()->IsA(UDungeonCanvasBlueprint::StaticClass()))
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_DungeonCanvas", "DUNGEON CANVAS");
	}
	return AppearanceInfo;
}

void FDungeonCanvasEditor::SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents) {
	FBlueprintEditor::SetupGraphEditorEvents(InGraph, InEvents);
	
	InEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FDungeonCanvasEditor::HandleCreateGraphActionMenu);
}

void FDungeonCanvasEditor::RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated) {
	if (InBlueprints.Num() == 1)
	{
		TSharedPtr<FDungeonCanvasEditor> ThisPtr(SharedThis(this));

		// Create the modes and activate one (which will populate with a real layout)
		TArray<TSharedRef<FApplicationMode>> TempModeList;
		TempModeList.Add(MakeShareable(new FDungeonCanvasApplicationModeCanvasGraph(ThisPtr)));

		for (const TSharedRef<FApplicationMode>& AppMode : TempModeList)
		{
			AddApplicationMode(AppMode->GetModeName(), AppMode);
		}

		const FName& DefaultModeName = DA::DungeonCanvas::Private::FDungeonCanvasApplicationModes::CanvasGraphMode;
		SetCurrentMode(DefaultModeName);

		// Activate our edit mode
		GetEditorModeManager().SetDefaultMode(DefaultModeName);
		GetEditorModeManager().ActivateMode(DefaultModeName);
	}
}

void FDungeonCanvasEditor::PostUndo(bool bSuccessful) {
	FBlueprintEditor::PostUndo(bSuccessful);

	OnDungeonCanvasChanged().Broadcast();
}

void FDungeonCanvasEditor::PostRedo(bool bSuccessful) {
	FBlueprintEditor::PostRedo(bSuccessful);
	
	OnDungeonCanvasChanged().Broadcast();
}

void FDungeonCanvasEditor::Compile() {
	FCompileScope CompileScope(&CompileGuard);

	DestroyInstance();
	RecompileMaterialTemplate();

	FBlueprintEditor::Compile();

	if (const UDungeonCanvasBlueprint* CanvasBP = Cast<UDungeonCanvasBlueprint>(GetBlueprintObj())) {
		UMaterialEditingLibrary::UpdateMaterialInstance(CanvasBP->MaterialInstance);
	}
}

void FDungeonCanvasEditor::SaveAsset_Execute() {
	UpdateAssetThumbnail();
	
	FBlueprintEditor::SaveAsset_Execute();
}


FName FDungeonCanvasEditor::GetToolkitFName() const {
	return FName("DungeonCanvasEditor");
}

FText FDungeonCanvasEditor::GetBaseToolkitName() const {
	return LOCTEXT("AppLabel", "Dungeon Canvas Editor");
}

FString FDungeonCanvasEditor::GetDocumentationLink() const {
	return FString();
}

FText FDungeonCanvasEditor::GetToolkitToolTipText() const {
	return GetToolTipTextForObject(GetBlueprintObj());
}

FLinearColor FDungeonCanvasEditor::GetWorldCentricTabColorScale() const {
	return FLinearColor(0.5f, 0.25f, 0.35f, 0.5f);
}

FString FDungeonCanvasEditor::GetWorldCentricTabPrefix() const {
	return LOCTEXT("WorldCentricTabPrefix", "Dungeon Canvas ").ToString();
}

void FDungeonCanvasEditor::InitToolMenuContext(FToolMenuContext& MenuContext) {
	FBlueprintEditor::InitToolMenuContext(MenuContext);
}

void FDungeonCanvasEditor::Tick(float DeltaTime) {
	FBlueprintEditor::Tick(DeltaTime);

	// Disable the graph (tick, init etc) when it's simulating. This happens when PIE is enabled
	const bool bIsGraphSimulating = (GEditor->bIsSimulatingInEditor || GEditor->PlayWorld != NULL);
	const bool bCanvasEnabled = !bIsGraphSimulating;
	if (CanvasInstance) {
		CanvasInstance->SetCanvasEnabled(bCanvasEnabled);
	}
	
	if (bCanvasEnabled && !CanvasInstance) {
		RefreshInstance(true);
	}
	
	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread) {
		PreviewScene.GetWorld()->Tick(LEVELTICK_All, DeltaTime);
	}

	if (bRecompileNextFrame) {
		bRecompileNextFrame = false;
		if (UDungeonCanvasBlueprint* CanvasBP = Cast<UDungeonCanvasBlueprint>(GetBlueprintObj())) {
			CanvasBP->Status = BS_Dirty;
			Compile();
		}
	}
}

TStatId FDungeonCanvasEditor::GetStatId() const {
	RETURN_QUICK_DECLARE_CYCLE_STAT(FDungeonCanvasEditor, STATGROUP_Tickables);
}

void FDungeonCanvasEditor::OnPropertyChanged(FString PropertyName, UDungeonEditorViewportProperties* Properties) {
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDungeonEditorViewportProperties, BuilderClass)) {
		RecreateDungeonBuilder(Properties);
		check(Properties->DungeonConfig);
		SKismetInspector::FShowDetailsOptions DetailOptions;
		DetailOptions.bForceRefresh = true;
		ShowDetailsForSingleObject(Properties, DetailOptions);

		if (DungeonCanvasToolbar.IsValid()) {
			DungeonCanvasToolbar->OnBuilderChanged();
		}
	}
}

void FDungeonCanvasEditor::RecreateDungeonBuilder(UDungeonEditorViewportProperties* InViewportProperties) {
	if (!InViewportProperties) {
		return;
	}
	if (!PreviewDungeon->GetBuilder() || PreviewDungeon->GetBuilder()->GetClass() != InViewportProperties->BuilderClass) {
		PreviewDungeon->SetBuilderClass(InViewportProperties->BuilderClass);
	}

	check(PreviewDungeon->GetBuilder() && PreviewDungeon->GetConfig());
	if (!InViewportProperties->DungeonConfig || InViewportProperties->DungeonConfig->GetClass() != PreviewDungeon->GetConfig()->GetClass()) {
		InViewportProperties->DungeonConfig = NewObject<UDungeonConfig>(InViewportProperties, PreviewDungeon->GetConfig()->GetClass());
		InViewportProperties->DungeonConfig->ConfigPropertyChanged.Unbind();
		InViewportProperties->DungeonConfig->ConfigPropertyChanged.BindUObject(InViewportProperties, &UDungeonEditorViewportProperties::PostEditChangeConfigProperty);
	}

	// Set the default dungeon config settings, so we can build it directly without manual intervention
	if (EditorDefaultSettings.IsValid()) {
		if (UGridFlowConfig* GridFlowConfig = Cast<UGridFlowConfig>(InViewportProperties->DungeonConfig)) {
			GridFlowConfig->GridFlow = EditorDefaultSettings->BuilderDefaultAssets.GridFlowGraph;
		}
		else if (UCellFlowConfig* CellFlowConfig = Cast<UCellFlowConfig>(InViewportProperties->DungeonConfig)) {
			CellFlowConfig->CellFlow = EditorDefaultSettings->BuilderDefaultAssets.CellFlowGraph;
		}
		else if (USnapGridFlowConfig* SGFConfig = Cast<USnapGridFlowConfig>(InViewportProperties->DungeonConfig)) {
			SGFConfig->FlowGraph = EditorDefaultSettings->BuilderDefaultAssets.SgfGraph;
			SGFConfig->ModuleDatabase = EditorDefaultSettings->BuilderDefaultAssets.SgfModDB;
		}
		else if (USnapMapDungeonConfig* SnapMapConfig = Cast<USnapMapDungeonConfig>(InViewportProperties->DungeonConfig)) {
			SnapMapConfig->DungeonFlowGraph = EditorDefaultSettings->BuilderDefaultAssets.SnapMapGraph;
			SnapMapConfig->ModuleDatabase = EditorDefaultSettings->BuilderDefaultAssets.SnapMapModDB;
		}
	}
	bRecompileNextFrame = true;
}

void FDungeonCanvasEditor::ShowDetailsForSingleObject(UObject* Object, const SKismetInspector::FShowDetailsOptions& Options) const {
	if (Object) {
		UE_LOG(LogCanvasEditor, Log, TEXT("Details Window Object Path: %s"), *Object->GetPathName(GetBlueprintObj()));
	}

	if (CompileGuard.IsActive()) {
		UE_LOG(LogCanvasEditor, Log, TEXT("Setting inspector while compile guard is active"));
		//return;
	}
	if (Inspector) {
		if (Object) {
			Inspector->ShowDetailsForObjects({ Object }, Options);
		}
		else {
			Inspector->ShowDetailsForObjects({});
		}
	}
}

void FDungeonCanvasEditor::FCompileGuard::Initialize(FDungeonCanvasEditor* InEditorPtr) {
	EditorPtr = InEditorPtr;
	ActiveScopeId.Invalidate();
	SelectedObjectPaths = {};
}

void FDungeonCanvasEditor::FCompileGuard::OnScopeStart(const FGuid& InScopeId) {
	if (!ActiveScopeId.IsValid()) {
		ActiveScopeId = InScopeId;
		SaveState();
	}
}

void FDungeonCanvasEditor::FCompileGuard::OnScopeEnd(const FGuid& InScopeId) {
	if (ActiveScopeId == InScopeId) {
		RestoreState();
		SelectedObjectPaths = {};
		ActiveScopeId.Invalidate();
	}
}

void FDungeonCanvasEditor::FCompileGuard::SaveState() {
	if (!EditorPtr) return;
	
	SelectedObjectPaths = {};
	
	TArray<TWeakObjectPtr<UObject>> SelectedObjects = EditorPtr->Inspector->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects) {
		if (SelectedObject.IsValid()) {
			SelectedObjectPaths.Add(SelectedObject->GetPathName(EditorPtr->GetBlueprintObj()));
		}
	} 
}

void FDungeonCanvasEditor::FCompileGuard::RestoreState() {
	if (!EditorPtr || !EditorPtr->GetBlueprintObj()) {
		return;
	}

	UBlueprint* OuterBlueprint = EditorPtr->GetBlueprintObj();
	TArray<UObject*> SelectedObjects;
	for (const FString& SelectedObjectPath : SelectedObjectPaths) {
		if (UObject* SelectedObject = FindObject<UObject>(OuterBlueprint, *SelectedObjectPath)) {
			SelectedObjects.Add(SelectedObject);
		}
	}
	
	if (SelectedObjects.Num() == 1 && SelectedObjects[0]->IsA<ADungeonCanvas>()) {
		EditorPtr->StartEditingDefaults( true, true );
		return;
	}
	
	TArray<UObject*> Objects;
	for (TWeakObjectPtr<UObject> ObjectPtr : SelectedObjects) {
		UObject* Object = ObjectPtr.Get();
		if (Object && Object->IsValidLowLevel()) {
			Objects.Add(Object);
		}
	}
	if (Objects.Num() > 0) {
		SKismetInspector::FShowDetailsOptions DetailOptions;
		DetailOptions.bForceRefresh = true;
		
		EditorPtr->Inspector->ShowDetailsForObjects(Objects, DetailOptions);
	}
}

void FDungeonCanvasEditor::RefreshInstance(bool bForceFullUpdate) {
	UpdateInstance(GetDungeonCanvasBlueprint(), bForceFullUpdate);
}

UDungeonCanvasBlueprint* FDungeonCanvasEditor::GetDungeonCanvasBlueprint() const {
	return Cast<UDungeonCanvasBlueprint>(GetBlueprintObj());
}

ADungeonCanvas* FDungeonCanvasEditor::GetInstance() const {
	return CanvasInstance;
}

void FDungeonCanvasEditor::CalculateTextureDimensions(int32& OutWidth, int32& OutHeight, int32& OutDepth, int32& OutArraySize, bool bInIncludeBorderSize) const {
	OutWidth = PreviewTexWidth;
	OutHeight = PreviewTexHeight;
	OutDepth = 0;
	OutArraySize = 0;
	const int32 BorderSize = GetDefault<UDungeonCanvasEditorSettings>()->TextureBorderEnabled ? 1 : 0;

	if (!PreviewTexWidth || !PreviewTexHeight)
	{
		OutWidth = 0;
		OutHeight = 0;
		OutDepth = 0;
		OutArraySize = 0;
		return;
	}

	// Fit is the same as fill, but doesn't scale up past 100%
	const EDungeonCanvasEditorZoomMode CurrentZoomMode = GetZoomMode();
	if (CurrentZoomMode == EDungeonCanvasEditorZoomMode::Fit || CurrentZoomMode == EDungeonCanvasEditorZoomMode::Fill)
	{
		const int32 MaxWidth = FMath::Max(CanvasViewport->GetViewport()->GetSizeXY().X - 2 * BorderSize, 0);
		const int32 MaxHeight = FMath::Max(CanvasViewport->GetViewport()->GetSizeXY().Y - 2 * BorderSize, 0);

		if (MaxWidth * PreviewTexHeight < MaxHeight * PreviewTexWidth)
		{
			OutWidth = MaxWidth;
			OutHeight = FMath::DivideAndRoundNearest(OutWidth * PreviewTexHeight, PreviewTexWidth);
		}
		else
		{
			OutHeight = MaxHeight;
			OutWidth = FMath::DivideAndRoundNearest(OutHeight * PreviewTexWidth, PreviewTexHeight);
		}

		// If fit, then we only want to scale down
		// So if our natural dimensions are smaller than the viewport, we can just use those
		if (CurrentZoomMode == EDungeonCanvasEditorZoomMode::Fit && (PreviewTexWidth < OutWidth || PreviewTexHeight < OutHeight))
		{
			OutWidth = PreviewTexWidth;
			OutHeight = PreviewTexHeight;
		}
	}
	else
	{
		OutWidth = static_cast<int32>(PreviewTexWidth * Zoom);
		OutHeight = static_cast<int32>(PreviewTexHeight * Zoom);
	}

	if (bInIncludeBorderSize)
	{
		OutWidth += 2 * BorderSize;
		OutHeight += 2 * BorderSize;
	}
}

ESimpleElementBlendMode FDungeonCanvasEditor::GetColourChannelBlendMode() const {
	// Add the red, green, blue, alpha and desaturation flags to the enum to identify the chosen filters
	uint32 Result = (uint32)SE_BLEND_RGBA_MASK_START;
	Result += 15;
	
	return (ESimpleElementBlendMode)Result;
}

EDungeonCanvasEditorSampling FDungeonCanvasEditor::GetSampling() const {
	return Sampling;
}

void FDungeonCanvasEditor::SetZoomMode(const EDungeonCanvasEditorZoomMode InZoomMode) {
	// Update our own zoom mode
	ZoomMode = InZoomMode;
	
	// And also save it so it's used for new texture editors
	UDungeonCanvasEditorSettings& Settings = *GetMutableDefault<UDungeonCanvasEditorSettings>();
	Settings.ZoomMode = ZoomMode;
	Settings.PostEditChange();
}

double FDungeonCanvasEditor::GetCustomZoomLevel() const {
	return Zoom;
}

void FDungeonCanvasEditor::SetCustomZoomLevel(double ZoomValue) {
	// snap to discrete steps so that if we are nearly at 1.0 or 2.0, we hit them exactly:
	//ZoomValue = FMath::GridSnap(ZoomValue, MinZoom/4.0);

	double LogZoom = log2(ZoomValue);
	// the mouse wheel zoom is quantized on ZoomFactorLogSteps
	//	but that's too chunky for the drag slider, give it more steps, but on the same quantization grid
	double QuantizationSteps = FDungeonCanvasEditorConstants::ZoomFactorLogSteps*2.0;
	double LogZoomQuantized = (1.0/QuantizationSteps) * (double)FMath::RoundToInt( QuantizationSteps * LogZoom );
	ZoomValue = pow(2.0,LogZoomQuantized);

	ZoomValue = FMath::Clamp(ZoomValue, FDungeonCanvasEditorConstants::MinZoom, FDungeonCanvasEditorConstants::MaxZoom);
	
	// set member variable "Zoom"
	Zoom = ZoomValue;

	// For now we also want to be in custom mode whenever this is changed
	SetZoomMode(EDungeonCanvasEditorZoomMode::Custom);
}

EDungeonCanvasEditorZoomMode FDungeonCanvasEditor::GetZoomMode() const {
	return ZoomMode;
}

double FDungeonCanvasEditor::CalculateDisplayedZoomLevel() const {
	// Avoid calculating dimensions if we're custom anyway
	if (GetZoomMode() == EDungeonCanvasEditorZoomMode::Custom) {
		return Zoom;
	}

	int32 DisplayWidth, DisplayHeight, DisplayDepth, DisplayArraySize;
	CalculateTextureDimensions(DisplayWidth, DisplayHeight, DisplayDepth, DisplayArraySize, false);
	if (PreviewTexHeight != 0) {
		return (double)DisplayHeight / PreviewTexHeight;
	}
	else if (PreviewTexWidth != 0) {
		return (double)DisplayWidth / PreviewTexWidth;
	}
	else {
		return 0;
	}
}

void FDungeonCanvasEditor::ZoomIn() {
	// mouse wheel zoom
	const double CurrentZoom = CalculateDisplayedZoomLevel();
	SetCustomZoomLevel(CurrentZoom * FDungeonCanvasEditorConstants::ZoomFactor);
}

void FDungeonCanvasEditor::ZoomOut() {
	const double CurrentZoom = CalculateDisplayedZoomLevel();
	SetCustomZoomLevel(CurrentZoom / FDungeonCanvasEditorConstants::ZoomFactor);
}

void FDungeonCanvasEditor::OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled) {
	FBlueprintEditor::OnBlueprintChangedImpl(InBlueprint, bIsJustBeingCompiled);
	DestroyInstance();
}

const TArray<UDungeonCanvasMaterialLayer*>* FDungeonCanvasEditor::GetMaterialLayerList() const {
	const UDungeonCanvasBlueprint* CanvasBP = GetDungeonCanvasBlueprint();
	return IsValid(CanvasBP) ? &CanvasBP->MaterialLayers : nullptr;
}

void FDungeonCanvasEditor::OnMaterialLayerSelectionChanged(UDungeonCanvasMaterialLayer* Item, ESelectInfo::Type SelectInfo) {
	ActiveMaterialLayer = Item;

	// If the inspector is empty, set it directly, otherwise, only if it's UI initiated
	if (Inspector->GetSelectedObjects().Num() == 0 || SelectInfo != ESelectInfo::Direct) {
		ShowDetailsForSingleObject(Item);
	}
}

void FDungeonCanvasEditor::AddNewMaterialLayer(const FText& InLayerName, const FText& InLayerDescription, const TSoftObjectPtr<UMaterialFunctionMaterialLayer>& InMaterialLayer, const TSoftObjectPtr<UMaterialFunctionMaterialLayerBlend>& InMaterialBlend) {
	static const FText DefaultLayerName = LOCTEXT("DefaultLayerName", "Effect Layer");

	if (UDungeonCanvasBlueprint* CanvasBP = GetDungeonCanvasBlueprint()) {
		UDungeonCanvasMaterialLayer* NewLayer = NewObject<UDungeonCanvasMaterialLayer>(CanvasBP);
		NewLayer->LayerName = InLayerName;
		NewLayer->LayerDescription = InLayerDescription;
		NewLayer->MaterialLayer = InMaterialLayer;
		if (InMaterialBlend.IsValid()) {
			NewLayer->MaterialBlend = InMaterialBlend;
		}
		else if (EditorDefaultSettings.IsValid()) {
			NewLayer->MaterialBlend = EditorDefaultSettings->DefaultLayerBlend;
		}
		
		CanvasBP->MaterialLayers.Insert(NewLayer, 0);
		CanvasBP->Modify();
		CanvasBP->Status = BS_Dirty;
		
		MaterialLayerList->RefreshListView();
		MaterialLayerList->SetItemSelection(NewLayer, ESelectInfo::OnMouseClick);

		Compile();
		
		SKismetInspector::FShowDetailsOptions DetailsOptions;
		DetailsOptions.bForceRefresh = true;
		ShowDetailsForSingleObject(NewLayer, DetailsOptions);
	}
}

void FDungeonCanvasEditor::OnMaterialLayerDelete(UDungeonCanvasMaterialLayer* Item) {
	if (!Item) return;
	
	static const FText Title = LOCTEXT("DADeleteCanvasMatLayerTitle", "Delete Layer?");
	const EAppReturnType::Type ReturnValue = FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No,
															LOCTEXT("DADeleteCanvasMatLayer","Are you sure you want to delete the layer?"),
															Title);

	if (ReturnValue == EAppReturnType::Yes) {
		// Delete the layer
		if (UDungeonCanvasBlueprint* CanvasBP = GetDungeonCanvasBlueprint()) {
			CanvasBP->MaterialLayers.Remove(Item);
			CanvasBP->Modify();

			ActiveMaterialLayer = nullptr;
			bRecompileNextFrame = true;
		}
	}
}

void FDungeonCanvasEditor::OnMaterialLayerReordered(UDungeonCanvasMaterialLayer* Source, UDungeonCanvasMaterialLayer* Dest) {
	if (UDungeonCanvasBlueprint* CanvasBP = GetDungeonCanvasBlueprint()) {
		int32 DestIndex = -1;
		if (!CanvasBP->MaterialLayers.Find(Dest, DestIndex)) {
			DestIndex = 0;
		}
		CanvasBP->MaterialLayers.Remove(Source);
		CanvasBP->MaterialLayers.Insert(Source, DestIndex);
		bRecompileNextFrame = true;
	}
}

FText FDungeonCanvasEditor::GetMaterialLayerListRowText(UDungeonCanvasMaterialLayer* Item) const {
	static const FText UnknownText = LOCTEXT("MissingLayerName", "Unknown");
	return Item ? Item->LayerName : UnknownText;
}

FText FDungeonCanvasEditor::GetLayerListRowDescriptionText(UDungeonCanvasMaterialLayer* Item) const {
	static const FText UnknownText = LOCTEXT("MissingLayerDescName", "Unknown");
	return Item ? Item->LayerDescription : UnknownText;
}

EVisibility FDungeonCanvasEditor::IsLayerListRowDescriptionVisible(UDungeonCanvasMaterialLayer* Item) const {
	return Item && !Item->LayerDescription.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedPtr<SWidget> FDungeonCanvasEditor::CreateMaterialLayerListItemWidget(UDungeonCanvasMaterialLayer* InMaterialLayer) {
	return
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(this, &FDungeonCanvasEditor::GetMaterialLayerListRowText, InMaterialLayer)
				.ColorAndOpacity_Lambda([InMaterialLayer]() {
					return InMaterialLayer && InMaterialLayer->bEnabled ? FLinearColor::White : FLinearColor::Gray;
				})
				.Font(FDungeonArchitectStyle::Get().GetFontStyle("DungeonArchitect.ListView.LargeFont"))
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(this, &FDungeonCanvasEditor::GetLayerListRowDescriptionText, InMaterialLayer)
				.Visibility(this, &FDungeonCanvasEditor::IsLayerListRowDescriptionVisible, InMaterialLayer)
				.ColorAndOpacity(FLinearColor::Gray)
				.Font(FDungeonArchitectStyle::Get().GetFontStyle("DungeonArchitect.ListView.Font"))
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([InMaterialLayer]() {
				return InMaterialLayer && InMaterialLayer->bEnabled
					? ECheckBoxState::Checked
					: ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this, InMaterialLayer](ECheckBoxState NewCheckboxState) {
				if (InMaterialLayer) {
					InMaterialLayer->bEnabled = (NewCheckboxState == ECheckBoxState::Checked);
					bRecompileNextFrame = true;
				}
			})
			.ToolTipText(LOCTEXT("LayerVisiblityTooltip", "Enable / Disable the material layer"))
			.CheckedImage(FAppStyle::Get().GetBrush("Icons.Visible"))
			.CheckedHoveredImage(FAppStyle::Get().GetBrush("Icons.Visible"))
			.CheckedPressedImage(FAppStyle::Get().GetBrush("Icons.Visible"))
			.UncheckedImage(FAppStyle::Get().GetBrush("Icons.Hidden"))
			.UncheckedHoveredImage(FAppStyle::Get().GetBrush("Icons.Hidden"))
			.UncheckedPressedImage(FAppStyle::Get().GetBrush("Icons.Hidden"))
		];
	
}

void FDungeonCanvasEditor::RecompileMaterialTemplate() {
	FDungeonCanvasEditorUtilities::CompileDungeonCanvasMaterialTemplate(GetDungeonCanvasBlueprint());
	
	DestroyInstance();
	FlushRenderingCommands();
}

void FDungeonCanvasEditor::UpdateAssetThumbnail() const {
	TSharedPtr<FViewport> PreviewViewport;
	if (CanvasViewport.IsValid()) {
		PreviewViewport = CanvasViewport->GetViewport();
	}

	if (PreviewViewport.IsValid()) {
		const FAssetData AssetData(GetBlueprintObj());
		TArray<FAssetData> ThemeAssetList;
		ThemeAssetList.Add(AssetData);

		FDungeonEditorUtils::CaptureThumbnailFromViewport(PreviewViewport.Get(), ThemeAssetList, [](const FColor& InColor) {
			FLinearColor LinearColor = InColor.ReinterpretAsLinear();
			return LinearColor.ToFColor(true);
		});
	}
}

TSharedRef<SWidget> FDungeonCanvasEditor::GetAddMaterialLayerMenu() {
	FMenuBuilder MenuBuilder(true, nullptr);
	if (EditorDefaultSettings.IsValid()) {
		MenuBuilder.BeginSection("MaterialLayersPresets", LOCTEXT("MaterialLayerPresetSection", "Material Layer Presets"));
		for (const FDungeonCanvasEditorMaterialLayerPreset& Preset : EditorDefaultSettings->MaterialLayerPresets) {
			MenuBuilder.AddMenuEntry(
				Preset.Title,
				Preset.Tooltip,
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([this, Preset]() {
					AddNewMaterialLayer(Preset.Title, Preset.Tooltip, Preset.MaterialLayer, Preset.MaterialBlend);
				})));
		}
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection("MaterialLayerCustom", LOCTEXT("MaterialLayerCustomSection", "Advanced"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("LayerNameBackground", "Custom Material Layer"),
			LOCTEXT("LayerNameBackgroundTooltip", "Add your own material layer and blend functions"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this]() {
				AddNewMaterialLayer(LOCTEXT("CustomLayerTitle", "Custom Layer"), LOCTEXT("CustomLayerTitle", "Custom Layer for user defined material functions"), nullptr, nullptr);
			})));
	}
	MenuBuilder.EndSection();


	return MenuBuilder.MakeWidget();
}

void FDungeonCanvasEditor::RandomizePreviewDungeon() const {
	UE_LOG(LogCanvasEditor, Log, TEXT("Randomize dungeon requested"));
	// Execute the blueprint event
	if (IsValid(CanvasInstance) && PreviewDungeon.IsValid()) {
		PreviewDungeon->RandomizeSeed();
		BuildPreviewDungeon();
		CanvasInstance->Initialize(false);
	}
	
}

void FDungeonCanvasEditor::AddReferencedObjects(FReferenceCollector& Collector) {
	FBlueprintEditor::AddReferencedObjects(Collector);
	if (IsValid(CanvasInstance)) {
		Collector.AddReferencedObject(CanvasInstance);
	}
	CanvasViewport->AddReferencedObjects(Collector);
}

void FDungeonCanvasEditor::NotifyPreChange(FProperty* PropertyAboutToChange) {
	FBlueprintEditor::NotifyPreChange(PropertyAboutToChange);
	if (UDungeonCanvasBlueprint* DungeonCanvasBlueprint = GetDungeonCanvasBlueprint())
	{
		DungeonCanvasBlueprint->Modify();
	}
}

void FDungeonCanvasEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) {
	FBlueprintEditor::NotifyPostChange(PropertyChangedEvent, PropertyThatChanged);

	const FName PropertyName = PropertyThatChanged ? PropertyThatChanged->GetFName() : NAME_None;
	
	const FString PropertyPath = PropertyThatChanged ? PropertyThatChanged->GetPathName() : "";
	UE_LOG(LogCanvasEditor, Log, TEXT("PostChange Path: %s"), *PropertyPath);
	
	if (const UObject* ObjectBeingEdited = PropertyChangedEvent.GetObjectBeingEdited(0)) {
		if (const UDungeonCanvasBlueprint* CanvasBlueprint = GetDungeonCanvasBlueprint()) {
			if (CanvasBlueprint->PreviewDungeonProperties == ObjectBeingEdited) {
				if (PropertyName == GET_MEMBER_NAME_CHECKED(UDungeonEditorViewportProperties, BuilderClass)) {
					RefreshInstance(true);
					ShowPreviewDungeonSettings();
				}
			}
		}
		if (GetClassDefaultObject() == ObjectBeingEdited) {
			RefreshInstance(true);
		}
	}
}

void FDungeonCanvasEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FEditPropertyChain* PropertyThatChanged) {
	UE_LOG(LogCanvasEditor, Log, TEXT("Chain prop changed"));
	
	auto Node = PropertyThatChanged->GetHead();
	while(Node)
	{
		if (const FProperty* Property = Node->GetValue())
		{
			const FName PropertyName = Property->GetFName();
			if (PropertyName == GET_MEMBER_NAME_CHECKED(UDungeonCanvasMaterialLayer, MaterialLayer)
				|| PropertyName == GET_MEMBER_NAME_CHECKED(UDungeonCanvasMaterialLayer, MaterialBlend))
			{
				UE_LOG(LogCanvasEditor, Log, TEXT("PostChainChange Path: %s"), *Property->GetPathName());
				bRecompileNextFrame = true;
				break;
			}
			
			if (PropertyChangedEvent.GetNumObjectsBeingEdited() > 0) {
				if (const ADungeonCanvas* CDO = Cast<ADungeonCanvas>(PropertyChangedEvent.GetObjectBeingEdited(0))) {
					if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive) {
						UE_LOG(LogCanvasEditor, Log, TEXT("PostChainChange CDO Path: %s"), *Property->GetPathName());
						bRecompileNextFrame = true;
						break;
					}
				}
			}
		}
		
		Node = Node->GetNextNode();
	}
}

void FDungeonCanvasEditor::BindCommands() {
	// Add Commands if needed.  Example below
	const FDungeonCanvasEditorCommands& Commands = FDungeonCanvasEditorCommands::Get();
	
	ToolkitCommands->MapAction(
		Commands.EditCanvasSettings,
		FExecuteAction::CreateSP(this, &FDungeonCanvasEditor::EditClassDefaults_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FDungeonCanvasEditor::IsDetailsPanelEditingClassDefaults));

	ToolkitCommands->MapAction(
	Commands.EditDungeonSettings,
	FExecuteAction::CreateSP(this, &FDungeonCanvasEditor::EditPreviewDungeonSettings_Clicked),
	FCanExecuteAction(),
	FIsActionChecked::CreateSP(this, &FDungeonCanvasEditor::IsDetailsPanelEditingDungeonPreviewSettings));
}

void FDungeonCanvasEditor::UndoAction() {
	GEditor->UndoTransaction();
}

void FDungeonCanvasEditor::RedoAction() {
	GEditor->RedoTransaction();
}

UObject* FDungeonCanvasEditor::GetClassDefaultObject() const {
	if (GetBlueprintObj() && GetBlueprintObj()->GeneratedClass) {
		return GetBlueprintObj()->GeneratedClass->GetDefaultObject();
	}
	return nullptr;
}

void FDungeonCanvasEditor::EditPreviewDungeonSettings_Clicked() {
	ShowPreviewDungeonSettings();
}

void FDungeonCanvasEditor::ShowPreviewDungeonSettings() {
	SKismetInspector::FShowDetailsOptions Options(LOCTEXT("PreviewDungeonInspectorText", "Preview Dungeon"), true);
	Options.bShowComponents = false;

	if (const UDungeonCanvasBlueprint* CanvasBlueprint = GetDungeonCanvasBlueprint()) {
		ShowDetailsForSingleObject(CanvasBlueprint->PreviewDungeonProperties, Options);
		TryInvokingDetailsTab(); 
	}
}

bool FDungeonCanvasEditor::IsDetailsPanelEditingDungeonPreviewSettings() const {
	if (const UDungeonCanvasBlueprint* CanvasBlueprint = GetDungeonCanvasBlueprint()) {
		if (CanvasBlueprint->PreviewDungeonProperties) {
			return Inspector->IsSelected(CanvasBlueprint->PreviewDungeonProperties);
		}
	}
	return false;
}

void FDungeonCanvasEditor::ExtendMenu() {
	if (MenuExtender.IsValid())
	{
		RemoveMenuExtender(MenuExtender);
		MenuExtender.Reset();
	}

	MenuExtender = MakeShareable(new FExtender);

	AddMenuExtender(MenuExtender);
}

void FDungeonCanvasEditor::ExtendToolbar() {
	// If the ToolbarExtender is valid, remove it before rebuilding it
	if (ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FDungeonCanvasEditor::FillToolbar)
	);
}

void FDungeonCanvasEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder) {
	ToolbarBuilder.BeginSection("Common");
	{
		//ToolbarBuilder.AddToolBarButton(FDungeonCanvasEditorCommands::Get().Build, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.OpenCinematic"));
	}
	ToolbarBuilder.EndSection();
}


void FDungeonCanvasEditor::DestroyInstance() {
	if (CanvasInstance && CanvasInstance->IsValidLowLevel()) {
		CanvasInstance->Destroy();
	}
	CanvasInstance = nullptr;
	PreviewBlueprintWeakPtr.Reset();
}

void FDungeonCanvasEditor::UpdateInstance(UBlueprint* InBlueprint, bool bInForceFullUpdate) {
	// If the Blueprint is changing
	if ((InBlueprint != PreviewBlueprintWeakPtr.Get()) || bInForceFullUpdate)
	{
		// Destroy the previous instance
		DestroyInstance();
		
		// Save the Blueprint we're creating a preview for
		PreviewBlueprintWeakPtr = Cast<UDungeonCanvasBlueprint>(InBlueprint);

		if (const UDungeonCanvasBlueprint* PreviewBlueprint = PreviewBlueprintWeakPtr.Get(); IsValid(PreviewBlueprint) && IsValid(PreviewBlueprint->GeneratedClass)) {
			FMakeClassSpawnableOnScope TemporarilySpawnable(PreviewBlueprint->GeneratedClass);
			if (UWorld* World = PreviewScene.GetWorld()) {
				// Recreate the preview dungeon actor
				if (PreviewDungeon.IsValid()) {
					PreviewDungeon->Destroy();
				}
				PreviewDungeon = World->SpawnActor<ADungeon>();

				UpdatePreviewActor(GetBlueprintObj(), true);
				CanvasInstance = Cast<ADungeonCanvas>(PreviewActorPtr.Get());
				if (CanvasInstance) {
					CanvasInstance->Dungeon = PreviewDungeon.Get();
					CanvasInstance->bEditorPreviewMode = true;	
				}
			}
		}

		// Set the debug object again, if it was debugging
		//GetBlueprintObj()->SetObjectBeingDebugged(CanvasInstance);

		// Broadcast the events
		OnDungeonCanvasChanged().Broadcast();

		// Execute the blueprint event
		if (IsValid(CanvasInstance) && PreviewDungeon.IsValid()) {
			BuildPreviewDungeon();
			CanvasInstance->Initialize();
		}

		MaterialLayerList->RefreshListView();
	}
}

void FDungeonCanvasEditor::BuildPreviewDungeon() const {
	if (!PreviewDungeon.IsValid()) {
		return;
	}

	if (const UDungeonCanvasBlueprint* CanvasBlueprint = GetDungeonCanvasBlueprint()) {
		if (const UDungeonEditorViewportProperties* PreviewViewportProperties = CanvasBlueprint->PreviewDungeonProperties) {
			// Set the builder instance and initialize it
			PreviewDungeon->SetBuilderClass(PreviewViewportProperties->BuilderClass);

			// copy over the config from the preview config over to the dungeon actor's config
			UDungeonConfig* DungeonConfig = PreviewDungeon->GetConfig();
			UEngine::CopyPropertiesForUnrelatedObjects(PreviewViewportProperties->DungeonConfig, DungeonConfig);
		}
	}

	UDungeonBuilder* DungeonBuilder = PreviewDungeon->GetBuilder();
	UDungeonModel* DungeonModel = PreviewDungeon->GetModel();
	UDungeonConfig* DungeonConfig = PreviewDungeon->GetConfig();

	// Flag the builder as a preview layout only builder. This will not spawn any items on the scene
	DungeonBuilder->SetLayoutOnlyPreviewBuild(true);
	
	if (DungeonModel->ShouldAutoResetOnBuild()) {
		DungeonModel->Reset();
	}
	
	if (DungeonBuilder->SupportsTheming()) {
		DungeonBuilder->BuildDungeon(PreviewDungeon.Get(), PreviewDungeon->GetWorld());
	}
	else {
		DungeonBuilder->BuildNonThemedDungeon(PreviewDungeon.Get(), nullptr, PreviewDungeon->GetWorld());
	}
}

/*
void FDungeonCanvasEditor::RefreshMaterialListView() const {
	MaterialLayerList->RefreshListView();
	if (UDungeonCanvasMaterialLayer* SelectedMaterialLayer = MaterialLayerList->GetSelectedItem()) {
		if (Inspector) {
			const UObject* SelectedObject{};
			{
				TArray<TWeakObjectPtr<>> InspectorObjects = Inspector->GetSelectedObjects();
				for (TWeakObjectPtr<> InspectorObject : InspectorObjects) {
					if (InspectorObject.IsValid()) {
						SelectedObject = InspectorObject.Get();
						break;
					}
				}
			}

			if (!SelectedObject) {
				Inspector->ShowDetailsForSingleObject(SelectedMaterialLayer);
			}
		}
	}
}
*/

FActionMenuContent FDungeonCanvasEditor::HandleCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed) {
	return OnCreateGraphActionMenu(InGraph, InNodePosition, InDraggedPins, bAutoExpand, InOnMenuClosed);
}

#undef LOCTEXT_NAMESPACE

