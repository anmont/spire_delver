//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/FlowEditor/FlowEditor.h"

#include "Core/Common/DungeonArchitectCommands.h"
#include "Core/Common/Utils/DungeonEditorConstants.h"
#include "Core/Common/Utils/DungeonEditorUtils.h"
#include "Core/Dungeon.h"
#include "Core/DungeonConfig.h"
#include "Core/Editors/FlowEditor/DomainEditors/FlowDomainEditor.h"
#include "Core/Editors/FlowEditor/FlowEditorCommands.h"
#include "Core/Editors/FlowEditor/FlowEditorSettings.h"
#include "Core/Editors/FlowEditor/Panels/Viewport/SFlowPreview3DViewport.h"
#include "Core/LevelEditor/Customizations/DungeonArchitectStyle.h"
#include "Frameworks/Flow/ExecGraph/FlowExecEdGraph.h"
#include "Frameworks/Flow/ExecGraph/FlowExecEdGraphSchema.h"
#include "Frameworks/Flow/ExecGraph/FlowExecGraphHandler.h"
#include "Frameworks/Flow/ExecGraph/FlowExecGraphScript.h"
#include "Frameworks/Flow/ExecGraph/FlowExecTaskStructs.h"
#include "Frameworks/Flow/ExecGraph/Nodes/FlowExecEdGraphNodeBase.h"
#include "Frameworks/Flow/FlowAssetBase.h"
#include "Frameworks/Flow/FlowProcessor.h"

#include "AssetEditorModeManager.h"
#include "EdGraph/EdGraph.h"
#include "FileHelpers.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/SlateDelegates.h"
#include "GraphEditor.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FlowEditor"

DEFINE_LOG_CATEGORY_STATIC(LogFlowEditor, Log, All);

const FName FFlowEditorBase::FFlowEditorTabs::ExecGraphID(TEXT("ExecGraph"));
const FName FFlowEditorBase::FFlowEditorTabs::DetailsID(TEXT("Details"));
const FName FFlowEditorBase::FFlowEditorTabs::ViewportID(TEXT("Viewport"));
const FName FFlowEditorBase::FFlowEditorTabs::PerformanceID(TEXT("Performance"));

void FFlowEditorBase::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost,
                                 UFlowAssetBase* FlowAsset) {
    GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseOtherEditors(FlowAsset, this);
    AssetBeingEdited = FlowAsset;
    EditorSettings = CreateEditorSettingsObject();
    EditorModeManager = MakeShared<FAssetEditorModeManager>();

    CreatePreviewViewport();
    CreatePropertyEditorWidget();
    CreateDomainEditors();
    CreateExecGraphEditorWidget();

    BindCommands();
    ExtendMenu();
    ExtendToolbar();

    UpgradeAsset();

    TSharedPtr<FTabManager::FLayout> Layout;
    if (RequiresCustomFrameLayout()) {
        Layout = CreateFrameLayout();
    }

    if (!Layout.IsValid()) {
        // Default layout
        Layout = FTabManager::NewLayout(ConstructLayoutName("FlowEditor_Layout_v1.0.2"))
            ->AddArea
            (
                FTabManager::NewPrimaryArea()
                ->SetOrientation(Orient_Vertical)
                ->Split
                (
                    FTabManager::NewSplitter()
                    ->SetSizeCoefficient(0.4f)
                    ->SetOrientation(Orient_Horizontal)
                    ->Split // Exec Graph
                    (
                        FTabManager::NewStack()
                        ->SetSizeCoefficient(0.65f)
                        ->AddTab(FFlowEditorTabs::ExecGraphID, ETabState::OpenedTab)
                        ->SetHideTabWell(true)
                    )
                    ->Split // Preview Viewport 3D
                    (
                        FTabManager::NewStack()
                        ->SetSizeCoefficient(0.35f)
                        ->AddTab(FFlowEditorTabs::ViewportID, ETabState::OpenedTab)
                        ->SetHideTabWell(true)
                    )
                )
                ->Split
                (
                    FTabManager::NewSplitter()
                    ->SetSizeCoefficient(0.6f)
                    ->SetOrientation(Orient_Horizontal)
                    ->Split // Details Tab
                    (
                        FTabManager::NewStack()
                        ->SetSizeCoefficient(0.15f)
                        ->AddTab(FFlowEditorTabs::DetailsID, ETabState::OpenedTab)
                    )
                    ->Split // Domain Editors
                    (
                        CreateDomainEditorLayout()
                    )
                )
            );
    }
    

    InitAssetEditor(Mode, InitToolkitHost, GetFlowEdAppName(), Layout.ToSharedRef(),
                    /*bCreateDefaultStandaloneMenu=*/ true, /*bCreateDefaultToolbar=*/ true, FlowAsset);

    SyncEdGraphNodeStates();
}

TSharedRef<FTabManager::FSplitter> FFlowEditorBase::CreateDomainEditorLayout() const {
    TSharedRef<FTabManager::FSplitter> Host = FTabManager::NewSplitter()
          ->SetSizeCoefficient(0.85f)
          ->SetOrientation(Orient_Horizontal);
    
    for (const IFlowDomainEditorPtr& DomainEditor : DomainEditors) {
        if (DomainEditor->IsVisualEditor()) {
            FFlowDomainEditorTabInfo TabInfo = DomainEditor->GetTabInfo();
            Host->Split (
                FTabManager::NewStack()
                ->AddTab(TabInfo.TabID, ETabState::OpenedTab)
                ->SetHideTabWell(true)
            );
        }
    }
    return Host;
}

TArray<IFlowDomainPtr> FFlowEditorBase::GetDomainList() const {
    TArray<IFlowDomainPtr> Domains;
    for (const IFlowDomainEditorPtr& DomainEditor : DomainEditors) {
        IFlowDomainPtr Domain = DomainEditor.IsValid() ? DomainEditor->GetDomain() : nullptr;
        if (Domain.IsValid()) {
            Domains.Add(DomainEditor->GetDomain());
        }
    }
    return Domains;
}

void FFlowEditorBase::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) {
    WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
        LOCTEXT("WorkspaceMenu_DungeonEditor", "Dungeon Editor"));
    auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

    InTabManager->RegisterTabSpawner(FFlowEditorTabs::ExecGraphID,
                                     FOnSpawnTab::CreateSP(this, &FFlowEditorBase::SpawnTab_ExecGraph))
                .SetDisplayName(LOCTEXT("ExecGraphTabLabel", "Execution Graph"))
                .SetGroup(WorkspaceMenuCategoryRef)
                .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

    InTabManager->RegisterTabSpawner(FFlowEditorTabs::DetailsID,
                                     FOnSpawnTab::CreateSP(this, &FFlowEditorBase::SpawnTab_Details))
                .SetDisplayName(LOCTEXT("DetailsTabLabel", "Details"))
                .SetGroup(WorkspaceMenuCategoryRef)
                .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

    InTabManager->RegisterTabSpawner(FFlowEditorTabs::ViewportID,
                                     FOnSpawnTab::CreateSP(this, &FFlowEditorBase::SpawnTab_Viewport))
                .SetDisplayName(LOCTEXT("PreviewTabLabel", "3D Preview"))
                .SetGroup(WorkspaceMenuCategoryRef)
                .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

    InTabManager->RegisterTabSpawner(FFlowEditorTabs::PerformanceID,
                                     FOnSpawnTab::CreateSP(this, &FFlowEditorBase::SpawnTab_Performance))
                .SetDisplayName(LOCTEXT("PerformanceTabLabel", "Performance Stats"))
                .SetGroup(WorkspaceMenuCategoryRef)
                .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

    // Register the domain editor tabs
    for (const IFlowDomainEditorPtr& DomainEditor : DomainEditors) {
        if (DomainEditor->IsVisualEditor()) {
            FFlowDomainEditorTabInfo TabInfo = DomainEditor->GetTabInfo();
            InTabManager->RegisterTabSpawner(TabInfo.TabID,
                    FOnSpawnTab::CreateSP(this, &FFlowEditorBase::SpawnTab_DomainEditor, DomainEditor->GetDomainID(), TabInfo.DisplayName))
                    .SetDisplayName(TabInfo.DisplayName)
                    .SetGroup(WorkspaceMenuCategoryRef)
                    .SetIcon(TabInfo.Icon);
        }
    }
}

void FFlowEditorBase::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) {
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

    InTabManager->UnregisterTabSpawner(FFlowEditorTabs::ExecGraphID);
    InTabManager->UnregisterTabSpawner(FFlowEditorTabs::DetailsID);
    InTabManager->UnregisterTabSpawner(FFlowEditorTabs::ViewportID);
    InTabManager->UnregisterTabSpawner(FFlowEditorTabs::PerformanceID);

    // Unregister the domain editors
    for (const IFlowDomainEditorPtr& DomainEditor : DomainEditors) {
        if (DomainEditor->IsVisualEditor()) {
            const FFlowDomainEditorTabInfo TabInfo = DomainEditor->GetTabInfo();
            InTabManager->UnregisterTabSpawner(TabInfo.TabID);
        }
    }
}

FText FFlowEditorBase::GetToolkitName() const {
    return FText::FromString(AssetBeingEdited->GetName());
}

FLinearColor FFlowEditorBase::GetWorldCentricTabColorScale() const {
    return FLinearColor::White;
}

FString FFlowEditorBase::GetDocumentationLink() const {
    // TODO: Fill me
    return TEXT("#");
}

void FFlowEditorBase::NotifyPreChange(FProperty* PropertyAboutToChange) {

}

void FFlowEditorBase::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent,
                                       FProperty* PropertyThatChanged) {

}

bool FFlowEditorBase::IsTickableInEditor() const {
    return true;
}

void FFlowEditorBase::Tick(float DeltaTime) {
    for (IFlowDomainEditorPtr DomainEditor : DomainEditors) {
        DomainEditor->Tick(DeltaTime);
    }
}

bool FFlowEditorBase::IsTickable() const {
    return true;
}

TStatId FFlowEditorBase::GetStatId() const {
    return TStatId();
}

void FFlowEditorBase::AddReferencedObjects(FReferenceCollector& Collector) {
    if (EditorSettings) {
        Collector.AddReferencedObject(EditorSettings);
    }
}

void FFlowEditorBase::UpgradeAsset() const {
    
}

UFlowAssetBase* FFlowEditorBase::GetAssetBeingEdited() const {
    return AssetBeingEdited;
}

void FFlowEditorBase::ShowObjectDetails(UObject* ObjectProperties, bool bForceRefresh /*= false*/) {
    if (!ObjectProperties) {
        // No object selected
        ObjectProperties = AssetBeingEdited;
    }

    if (PropertyEditor.IsValid()) {
        PropertyEditor->SetObject(ObjectProperties, bForceRefresh);
    }
}

UFlowEditorSettings* FFlowEditorBase::CreateEditorSettingsObject() const {
    return NewObject<UFlowEditorSettings>();
}

void FFlowEditorBase::CreateDomainEditors() {
    DomainEditors.Reset();
    DomainMediators.Reset();

    CreateDomainEditorsImpl();

    FDomainEdInitSettings InitSettings;
    InitSettings.PropertyEditor = PropertyEditor;
    InitSettings.EditorModeManager = EditorModeManager;
	
    // Initialize the domain editors
    for (const IFlowDomainEditorPtr& DomainEditor : DomainEditors) {
        if (DomainEditor.IsValid()) {
            DomainEditor->Initialize(InitSettings);
        }
    }
}

void FFlowEditorBase::ExtendMenu() {

}

void FFlowEditorBase::ExtendToolbar() {
    TSharedPtr<FExtender> ToolbarExtender(new FExtender);
    ToolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::Before,
        GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateSP(this, &FFlowEditorBase::FillToolbarCore)
    );
    ToolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::After,
        GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateSP(this, &FFlowEditorBase::FillToolbarMisc)
        );
    ToolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::After,
        GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateSP(this, &FFlowEditorBase::FillToolbarSupport)
    );
    AddToolbarExtender(ToolbarExtender);

}

void FFlowEditorBase::CreateExecGraphEditorWidget() {
    // Create the appearance info
    FGraphAppearanceInfo AppearanceInfo;
    AppearanceInfo.CornerText = LOCTEXT("FlowExecGraphBranding", "Execution Graph");
    ExecGraphHandler = MakeShareable(new FFlowExecGraphHandler);
    ExecGraphHandler->Bind();
    ExecGraphHandler->SetPropertyEditor(PropertyEditor);
    ExecGraphHandler->OnNodeSelectionChanged.BindRaw(this, &FFlowEditorBase::OnExecNodeSelectionChanged);
    ExecGraphHandler->OnNodeDoubleClicked.BindRaw(this, &FFlowEditorBase::OnExecNodeDoubleClicked);

    ExecGraphEditor = SNew(SGraphEditor)
		.AdditionalCommands(ExecGraphHandler->GraphEditorCommands)
		.Appearance(AppearanceInfo)
		.GraphToEdit(AssetBeingEdited->ExecEdGraph)
		.IsEditable(true)
		.ShowGraphStateOverlay(false)
		.GraphEvents(ExecGraphHandler->GraphEvents);

    // Setup the schema domains
    {
        const UFlowExecEdGraphSchema* ExecGraphSchema = Cast<UFlowExecEdGraphSchema>(AssetBeingEdited->ExecEdGraph->GetSchema());
        if (ExecGraphSchema) {
            TArray<IFlowDomainWeakPtr> SchemaDomains;
            for (const IFlowDomainEditorPtr& DomainEditor : DomainEditors) {
                SchemaDomains.Add(DomainEditor->GetDomain());
            }            
            ExecGraphSchema->SetAllowedDomains(SchemaDomains);
        }
    }
    
    ExecGraphHandler->SetGraphEditor(ExecGraphEditor);
}

void FFlowEditorBase::CreatePropertyEditorWidget() {
    FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs DetailsViewArgs;
    DetailsViewArgs.bUpdatesFromSelection = false;
    DetailsViewArgs.bLockable = false;
    DetailsViewArgs.bAllowSearch = true;
    DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    DetailsViewArgs.bHideSelectionTip = true;
    DetailsViewArgs.NotifyHook = this;

    PropertyEditor = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
}

void FFlowEditorBase::CreatePreviewViewport() {
    PreviewViewport =
        SNew(SFlowPreview3DViewport)
		.FlowAsset(AssetBeingEdited)
		.FlowEditor(SharedThis(this));

    ADungeon* PreviewDungeon = CreatePreviewDungeon(PreviewViewport->GetWorld());
    PreviewViewport->SetPreviewDungeon(PreviewDungeon);
}

void FFlowEditorBase::CompileExecGraph() const {
    if (AssetBeingEdited) {
        FFlowExecScriptCompiler::Compile(AssetBeingEdited->ExecEdGraph, AssetBeingEdited->ExecScript);
        AssetBeingEdited->Modify();
    }
}

void FFlowEditorBase::ExecuteGraph() {
    if (!AssetBeingEdited || !AssetBeingEdited->ExecEdGraph) {
        ExecGraphProcessor = nullptr;
        return;
    }

    int32 Seed = 0;
    if (EditorSettings) {
        if (EditorSettings->bRandomizeSeedOnBuild) {
            EditorSettings->Seed = FMath::Rand();
        }
        Seed = EditorSettings->Seed;
    }

    FRandomStream Random(Seed);
    //UE_LOG(GridFlowEditor, Log, TEXT("Seed: %d"), Random.GetInitialSeed());

    CompileExecGraph();
    ExecGraphProcessor = MakeShareable(new FFlowProcessor);

    // Register the domains
    for (const TSharedPtr<IFlowDomainEditor>& DomainEditor : DomainEditors) {
        IFlowDomainPtr Domain = DomainEditor->GetDomain();
        if (Domain.IsValid()) {
            ConfigureDomainObject(Domain);
            ExecGraphProcessor->RegisterDomain(Domain);
        }
    } 

    const int32 MaxExecTries = FMath::Clamp(EditorSettings->MaxBuildRetries, 1, 1000);
    int32 NumTries = 0;
    int32 NumTimeouts = 0;
    const int32 NumAllowedTimeouts = GeNumAllowedFlowExecTimeouts();
    while (NumTries < MaxExecTries) {
        FFlowProcessorSettings ProcessorSettings;
        ProcessorSettings.SerializedAttributeList = EditorSettings->ParameterOverrides;
        FFlowProcessorResult Result = ExecGraphProcessor->Process(AssetBeingEdited->ExecScript, Random, ProcessorSettings);
        if (Result.ExecResult == EFlowTaskExecutionResult::Success) {
            if (FFlowExecutionOutput* ResultNodeState = ExecGraphProcessor->GetNodeStatePtr(AssetBeingEdited->ExecScript->ResultNode->NodeId)) {
                FinalizeFlowGraphExecution(*ResultNodeState, Result.ExecResult);
            }
        }
        
        if (Result.ExecResult == EFlowTaskExecutionResult::Success) {
            break;
        }

        if (Result.ExecResult == EFlowTaskExecutionResult::FailHalt) {
            bool bHalt = true;

            if (Result.FailReason == EFlowTaskExecutionFailureReason::Timeout) {
                NumTimeouts++;
                if (NumTimeouts <= NumAllowedTimeouts) {
                    // Continue despite the timeout
                    bHalt = false;
                }
            }

            if (bHalt) {
                break;
            }
        }
        
        NumTries++;
    }

    SyncEdGraphNodeStates();
    AssetBeingEdited->ExecEdGraph->NotifyGraphChanged();

    // Update the preview graph
    FGuid PreviewNodeId;
    if (AssetBeingEdited->ExecScript && AssetBeingEdited->ExecScript->ResultNode) {
        PreviewNodeId = AssetBeingEdited->ExecScript->ResultNode->NodeId;
    }
    
    UpdatePreviewGraphs(PreviewNodeId);
    RecenterOutputGraphs(PreviewNodeId);

    if (ShouldBuildPreviewDungeon()) {
        RebuildDungeon();
    }
    
    // Collect garbage to clear out the destroyed level
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

void FFlowEditorBase::SyncEdGraphNodeStates() const {
    if (!AssetBeingEdited) return;

    UEdGraph* EdGraph = AssetBeingEdited->ExecEdGraph;
    if (!EdGraph) return;

    for (UEdGraphNode* EdNode : EdGraph->Nodes) {
        if (UFlowExecEdGraphNodeBase* FlowEdNode = Cast<UFlowExecEdGraphNodeBase>(EdNode)) {
            FlowEdNode->ErrorMessage = "";
            FlowEdNode->ExecutionStage = EFlowTaskExecutionStage::NotExecuted;
            FlowEdNode->ExecutionResult = EFlowTaskExecutionResult::Success;
        }
    }

    if (ExecGraphProcessor.IsValid()) {
        // Update the execution states on the Exec EdGraph 
        for (UEdGraphNode* EdNode : EdGraph->Nodes) {
            if (UFlowExecEdGraphNodeBase* FlowEdNode = Cast<UFlowExecEdGraphNodeBase>(EdNode)) {
                FFlowExecutionOutput NodeState;
                if (ExecGraphProcessor->GetNodeState(FlowEdNode->NodeGuid, NodeState)) {
                    FlowEdNode->ExecutionResult = NodeState.ExecutionResult;
                    FlowEdNode->ErrorMessage = NodeState.ErrorMessage;
                    FlowEdNode->ExecutionStage = ExecGraphProcessor->GetNodeExecStage(FlowEdNode->NodeGuid);
                }
            }
        }
    }
}

void FFlowEditorBase::OnExecNodeSelectionChanged(const TSet<class UObject*>& InSelectedObjects) {
    FGuid PreviewNodeId;
    // Get the selected exec node
    bool bFoundPreviewNode = false;
    for (UObject* NodeObj : InSelectedObjects) {
        if (UFlowExecEdGraphNodeBase* ExecNode = Cast<UFlowExecEdGraphNodeBase>(NodeObj)) {
            PreviewNodeId = ExecNode->NodeGuid;
            bFoundPreviewNode = true;
            break;
        }
    }

    // If we didn't find a suitable node, show the preview Node
    if (!bFoundPreviewNode) {
        if (AssetBeingEdited->ExecScript && AssetBeingEdited->ExecScript->ResultNode) {
            PreviewNodeId = AssetBeingEdited->ExecScript->ResultNode->NodeId;
        }
    }

    UpdatePreviewGraphs(PreviewNodeId);
}

void FFlowEditorBase::UpdatePreviewGraphs(const FGuid& NodeId) {
    const FFlowExecNodeStatePtr State = GetNodeState(NodeId);
    if (State.IsValid()) {
        for (IFlowDomainEditorPtr DomainEditor : DomainEditors) {
            DomainEditor->Build(State);
        }
    }
}

void FFlowEditorBase::OnExecNodeDoubleClicked(UEdGraphNode* InNode) {
    if (UFlowExecEdGraphNodeBase* ExecNode = Cast<UFlowExecEdGraphNodeBase>(InNode)) {
        const FGuid NodeId = ExecNode->NodeGuid;
        if (ExecGraphProcessor.IsValid()) {
            const EFlowTaskExecutionStage ExecStage = ExecGraphProcessor->GetNodeExecStage(NodeId);
            if (ExecStage == EFlowTaskExecutionStage::Executed) {
                FFlowExecutionOutput NodeExecOutput;
                if (ExecGraphProcessor->GetNodeState(NodeId, NodeExecOutput)) {
                    if (NodeExecOutput.ExecutionResult == EFlowTaskExecutionResult::Success) {
                        if (NodeExecOutput.State.IsValid()) {
                            for (IFlowDomainEditorPtr DomainEditor : DomainEditors) {
                                DomainEditor->RecenterView(NodeExecOutput.State);
                            }
                        }
                    }
                }
            }
        }
    }
}

void FFlowEditorBase::SaveAsset_Execute() {
    UPackage* Package = AssetBeingEdited->GetOutermost();
    if (Package) {
        CompileExecGraph();
        UpdateAssetThumbnail();

        TArray<UPackage*> PackagesToSave;
        PackagesToSave.Add(Package);

        FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
    }
}

TSharedRef<SDockTab> FFlowEditorBase::SpawnTab_ExecGraph(const FSpawnTabArgs& Args) {
    return SNew(SDockTab)
        .Label(LOCTEXT("ExecGraphTab_Title", "Execution Graph"))
        [
            SNew(SOverlay)
            +SOverlay::Slot()
            [
                ExecGraphEditor.ToSharedRef()
            ]
            
            // Bottom-right corner text indicating the type of tool
		    +SOverlay::Slot()
		    .Padding(10)
		    .VAlign(VAlign_Bottom)
		    .HAlign(HAlign_Left)
		    [
			    SNew(STextBlock)
			    .Visibility( EVisibility::HitTestInvisible )
			    .TextStyle( FDungeonArchitectStyle::Get(), "FlowEditor.ExecGraph.EditorBranding" )
			    .Text(this, &FFlowEditorBase::GetEditorBrandingText)
		    ]
        ];
}

TSharedRef<SDockTab> FFlowEditorBase::SpawnTab_Details(const FSpawnTabArgs& Args) {
    // Spawn the tab
    return SNew(SDockTab)
        .Label(LOCTEXT("DetailsTab_Title", "Details"))
        [
            PropertyEditor.ToSharedRef()
        ];
}

TSharedRef<SDockTab> FFlowEditorBase::SpawnTab_Viewport(const FSpawnTabArgs& Args) {
    // Spawn the tab
    return SNew(SDockTab)
        .Label(LOCTEXT("Preview3DTab_Title", "Preview 3D"))
        [
            PreviewViewport.ToSharedRef()
        ];
}

TSharedRef<SDockTab> FFlowEditorBase::SpawnTab_Performance(const FSpawnTabArgs& Args) {
    const TSharedRef<SDockTab> DockTab =
        SNew(SDockTab)
        .Label(LOCTEXT("PerformanceTab_Title", "Performance Stats"));

    const TSharedRef<SWidget> TestRunnerWidget = CreatePerfWidget(DockTab, Args.GetOwnerWindow());
    DockTab->SetContent(TestRunnerWidget);
    return DockTab;
}

TSharedRef<SWidget> FFlowEditorBase::CreatePerfWidget(const TSharedRef<SDockTab> DockTab, TSharedPtr<SWindow> OwnerWindow) {
    return SNullWidget::NullWidget;
}

int32 FFlowEditorBase::GeNumAllowedFlowExecTimeouts() const {
    return EditorSettings ? EditorSettings->NumTimeoutsRetriesAllowed : 0;
}

TSharedRef<SDockTab> FFlowEditorBase::SpawnTab_DomainEditor(const FSpawnTabArgs& Args, FName DomainID, FText Title) {
    TSharedPtr<SWidget> Content;
    for (IFlowDomainEditorPtr DomainEditor : DomainEditors) {
        if (DomainEditor->GetDomainID() == DomainID) {
            Content = DomainEditor->GetContentWidget();
            break;
        }
    }
    if (!Content.IsValid()) {
        Content = SNullWidget::NullWidget;
    }
    
    return SNew(SDockTab)
        .Label(Title)
        [
            Content.ToSharedRef()
        ];
}

void FFlowEditorBase::BindCommands() {
    // Bind flow editor commands
    {
        const FFlowEditorCommands& Commands = FFlowEditorCommands::Get();
        ToolkitCommands->MapAction(
            Commands.Build,
            FExecuteAction::CreateSP(this, &FFlowEditorBase::ExecuteGraph));

        ToolkitCommands->MapAction(
            Commands.Performance,
            FExecuteAction::CreateSP(this, &FFlowEditorBase::HandleShowPerformanceDialog));
    }

    // Bind help / support commands
    {
        const FDungeonArchitectCommands& HelpCommands = FDungeonArchitectCommands::Get();
        ToolkitCommands->MapAction(
            HelpCommands.OpenDocumentationWebsite,
            FExecuteAction::CreateStatic(&FDungeonEditorUtils::HandleOpenURL, FDungeonEditorConstants::URL_DOCS));

        ToolkitCommands->MapAction(
                HelpCommands.OpenSupportForums,
                FExecuteAction::CreateStatic(&FDungeonEditorUtils::HandleOpenURL, FDungeonEditorConstants::URL_FORUMS));
    
        ToolkitCommands->MapAction(
            HelpCommands.OpenSupportDiscord,
            FExecuteAction::CreateStatic(&FDungeonEditorUtils::HandleOpenURL, FDungeonEditorConstants::URL_DISCORD));
    }
    
    SettingsActionList = MakeShareable(new FUICommandList);
    SettingsActionList->MapAction(
        FFlowEditorCommands::Get().ShowEditorSettings,
        FExecuteAction::CreateSP(this, &FFlowEditorBase::HandleShowEditorSettings)
    );
    SettingsActionList->MapAction(
        FFlowEditorCommands::Get().ShowPreviewDungeonSettings, 
        FExecuteAction::CreateSP(this, &FFlowEditorBase::HandleShowDungeonSettings)
    );

}

void FFlowEditorBase::RecenterOutputGraphs(const FGuid& InNodeId) {
    const FFlowExecNodeStatePtr State = GetNodeState(InNodeId);
    if (State.IsValid()) {
        for (IFlowDomainEditorPtr DomainEditor : DomainEditors) {
            DomainEditor->RecenterView(State);
        }
    }
}

UDungeonConfig* FFlowEditorBase::GetDungeonConfig() const {
    if (PreviewViewport.IsValid()) {
        ADungeon* Dungeon = PreviewViewport->GetPreviewDungeon();
        if (Dungeon) {
            return Dungeon->GetConfig();
        }
    }
    return nullptr;
}

void FFlowEditorBase::RebuildDungeon() {
    if (PreviewViewport.IsValid()) {
        if (ADungeon* Dungeon = PreviewViewport->GetPreviewDungeon()) {
            if (UDungeonConfig* Config = Dungeon->GetConfig()) {
                Config->Seed = EditorSettings ? EditorSettings->Seed : 0;
                InitDungeonConfig(Config);
                Dungeon->BuildDungeon();
            }
        }
    }
}

void FFlowEditorBase::UpdateAssetThumbnail() {
    if (!AssetBeingEdited) {
        // Not asset is currently being edited
        return;
    }

    const int32 ThumbSize = 256;
    for (IFlowDomainEditorPtr DomainEditor : DomainEditors) {
        if (DomainEditor->CanSaveThumbnail()) {
            const FAssetData AssetData = FAssetData(AssetBeingEdited);
            DomainEditor->SaveThumbnail(AssetData, ThumbSize);
            break;
        }
    }
}

FFlowExecNodeStatePtr FFlowEditorBase::GetNodeState(const FGuid& InNodeId) const {
    if (ExecGraphProcessor.IsValid()) {
        const EFlowTaskExecutionStage ExecStage = ExecGraphProcessor->GetNodeExecStage(InNodeId);
        if (ExecStage == EFlowTaskExecutionStage::Executed) {
            FFlowExecutionOutput NodeExecOutput;
            if (ExecGraphProcessor->GetNodeState(InNodeId, NodeExecOutput)) {
                if (NodeExecOutput.ExecutionResult == EFlowTaskExecutionResult::Success) {
                    return NodeExecOutput.State;
                }
            }
        }
    }
    return nullptr;
}

void FFlowEditorBase::HandleShowPerformanceDialog() {
    if (TabManager.IsValid()) {
        TabManager->TryInvokeTab(FFlowEditorTabs::PerformanceID);
    }
}

void FFlowEditorBase::OnTestRunnerServiceStarted() {
    // Compile the Execution graph so we have the latest changes for the test runner
    CompileExecGraph();
}

FName FFlowEditorBase::ConstructLayoutName(const FString& Version) const {
    const FName AppName = GetFlowEdAppName();
    const FString LayoutName = FString::Printf(TEXT("Standalone_Flow_Ed_%s_%s"), *AppName.ToString(), *Version);
    return *LayoutName;
}

void FFlowEditorBase::HandleShowEditorSettings() {
    ShowObjectDetails(EditorSettings);
}

void FFlowEditorBase::HandleShowDungeonSettings() {
    if (PreviewViewport.IsValid()) {
        ADungeon* DungeonActor = PreviewViewport->GetPreviewDungeon();
        ShowObjectDetails(DungeonActor);
    }
}

TSharedRef<SWidget> FFlowEditorBase::HandleShowSettingsDropDownMenu() const {
    FMenuBuilder MenuBuilder(true, SettingsActionList);

    MenuBuilder.BeginSection("DA-Tools", LOCTEXT("DAHeader", "Dungeon Architect"));
    MenuBuilder.AddMenuEntry(FFlowEditorCommands::Get().ShowEditorSettings);
    MenuBuilder.AddMenuEntry(FFlowEditorCommands::Get().ShowPreviewDungeonSettings);
    MenuBuilder.EndSection();

    return MenuBuilder.MakeWidget();
}

void FFlowEditorBase::FillToolbarCore(FToolBarBuilder& ToolbarBuilder) const {
    ToolbarBuilder.BeginSection("Build");
    {
        ToolbarBuilder.AddToolBarButton(FFlowEditorCommands::Get().Build);
    }
    ToolbarBuilder.EndSection();
}

void FFlowEditorBase::FillToolbarMisc(FToolBarBuilder& ToolbarBuilder) const {
    ToolbarBuilder.BeginSection("Settings");
    {
        ToolbarBuilder.AddToolBarButton(FFlowEditorCommands::Get().Performance);
        ToolbarBuilder.AddComboButton(FUIAction(),
                                      FOnGetContent::CreateRaw(this, &FFlowEditorBase::HandleShowSettingsDropDownMenu),
                                      LOCTEXT("SettingsLabel", "Settings"),
                                      LOCTEXT("SettingsTooltip", "Show Settings"),
                                      FSlateIcon(FDungeonArchitectStyle::GetStyleSetName(), "FlowEditor.Settings"));
    }
    ToolbarBuilder.EndSection();
}

void FFlowEditorBase::FillToolbarSupport(FToolBarBuilder& ToolbarBuilder) const {
    ToolbarBuilder.BeginSection("Support");
    {
        ToolbarBuilder.AddToolBarButton(FDungeonArchitectCommands::Get().OpenDocumentationWebsite);
        ToolbarBuilder.AddToolBarButton(FDungeonArchitectCommands::Get().OpenSupportForums);
        ToolbarBuilder.AddToolBarButton(FDungeonArchitectCommands::Get().OpenSupportDiscord);
    }
    ToolbarBuilder.EndSection();
}

TSharedPtr<IDetailsView> FFlowEditorBase::GetPropertyEditor() const { return PropertyEditor; }

void FFlowEditorBase::CreateEditorModeManager() {
    
}

#undef LOCTEXT_NAMESPACE

