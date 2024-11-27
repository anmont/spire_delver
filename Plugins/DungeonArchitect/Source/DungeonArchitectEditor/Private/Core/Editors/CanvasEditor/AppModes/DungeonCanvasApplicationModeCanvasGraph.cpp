//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/CanvasEditor/AppModes/DungeonCanvasApplicationModeCanvasGraph.h"

#include "Core/Editors/CanvasEditor/AppModes/DungeonCanvasApplicationModes.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditor.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditorToolbar.h"
#include "Core/Editors/CanvasEditor/Viewport/SDungeonCanvasEditorViewport.h"

#include "BlueprintEditorTabs.h"
#include "SBlueprintEditorToolbar.h"

#define LOCTEXT_NAMESPACE "DungeonCavnasGraphMode"


class FDungeonCanvasViewportTabSummoner : public FWorkflowTabFactory {
public:
	FDungeonCanvasViewportTabSummoner(TSharedPtr<FDungeonCanvasEditor> InBlueprintEditor)
		: FWorkflowTabFactory(TabID, InBlueprintEditor)
		, BlueprintEditor(InBlueprintEditor)
	{
		bIsSingleton = true;
		TabLabel = LOCTEXT("ViewportLabel", "Viewport");
		TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports");
		ViewMenuDescription = LOCTEXT("TooltipMenu_Viewport", "Preview viewport");
		ViewMenuTooltip = LOCTEXT("TooltipMenu_Viewport", "Preview viewport");
	}

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override {
		const TSharedPtr<FDungeonCanvasEditor> DungeonCanvasEditor = BlueprintEditor.Pin();
		if (DungeonCanvasEditor.IsValid()) {
			return DungeonCanvasEditor->GetCanvasViewport().ToSharedRef();
		}
		return SNullWidget::NullWidget;
	}

public:
	static const FName TabID;
	
private:
	TWeakPtr<FDungeonCanvasEditor> BlueprintEditor;
};
const FName FDungeonCanvasViewportTabSummoner::TabID = TEXT("DungeonCanvasViewport");


class FDungeonCanvasMaterialLayerTabSummoner : public FWorkflowTabFactory {
public:
	FDungeonCanvasMaterialLayerTabSummoner(TSharedPtr<FDungeonCanvasEditor> InBlueprintEditor)
		: FWorkflowTabFactory(TabID, InBlueprintEditor)
		, BlueprintEditor(InBlueprintEditor)
	{
		bIsSingleton = true;
		TabLabel = LOCTEXT("MaterialLayersLabel", "Effect Layers");
		TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Layers");
		ViewMenuDescription = LOCTEXT("TooltipMenu_Viewport", "Effect Layers");
		ViewMenuTooltip = LOCTEXT("TooltipMenu_Viewport", "Effect Layers");
	}

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override {
		const TSharedPtr<FDungeonCanvasEditor> DungeonCanvasEditor = BlueprintEditor.Pin();
		if (DungeonCanvasEditor.IsValid()) {
			return DungeonCanvasEditor->GetMaterialLayerListWidget().ToSharedRef();
		}
		return SNullWidget::NullWidget;
	}

public:
	static const FName TabID;
	
private:
	TWeakPtr<FDungeonCanvasEditor> BlueprintEditor;
};
const FName FDungeonCanvasMaterialLayerTabSummoner::TabID = TEXT("DungeonCanvasMaterialLayer");

FDungeonCanvasApplicationModeCanvasGraph::FDungeonCanvasApplicationModeCanvasGraph(TSharedPtr<FDungeonCanvasEditor> InDungeonCanvasEditor)
	: FDungeonCanvasApplicationModeBase(InDungeonCanvasEditor, DA::DungeonCanvas::Private::FDungeonCanvasApplicationModes::CanvasGraphMode)
{
	WorkspaceMenuCategory = FWorkspaceItem::NewGroup(LOCTEXT("WorkspaceMenu_CanvasGraph", "Canvas Graph"));

	TabLayout = FTabManager::NewLayout("DungeonCanvasBlueprintEditor_Graph_Layout_v0.0.22")
		->AddArea(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.15f)
					->Split(
						FTabManager::NewStack()->SetSizeCoefficient(0.4f)
						->AddTab(FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab)
					)
					->Split(
						FTabManager::NewStack()->SetSizeCoefficient(0.25f)
						->AddTab(FDungeonCanvasMaterialLayerTabSummoner::TabID, ETabState::OpenedTab)
					)
					->Split(
						FTabManager::NewStack()->SetSizeCoefficient(0.35f)
						->AddTab(FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab)
						->AddTab(FBlueprintEditorTabs::PaletteID, ETabState::ClosedTab)
						->SetForegroundTab(FBlueprintEditorTabs::DetailsID)
					)
				)
				->Split(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.40f)
					->Split(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.80f)
						->AddTab("Document", ETabState::ClosedTab)
					)
					->Split(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.20f)
						->AddTab(FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab)
						->AddTab(FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab)
					)
				)
				->Split(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.45f)
					->Split(
						FTabManager::NewStack()
						->AddTab(FDungeonCanvasViewportTabSummoner::TabID, ETabState::OpenedTab)
					)
				)
			)
		);
	
	TabFactories.RegisterFactory(MakeShareable(new FDungeonCanvasViewportTabSummoner(InDungeonCanvasEditor)));
	TabFactories.RegisterFactory(MakeShareable(new FDungeonCanvasMaterialLayerTabSummoner(InDungeonCanvasEditor)));
	
	//Make sure we start with new one
	ToolbarExtender = MakeShareable(new FExtender);
	
	InDungeonCanvasEditor->GetDungeonCanvasToolbarBuilder()->AddDungeonCanvasBlueprintEditorModesToolbar(ToolbarExtender);

	if (UToolMenu* Toolbar = InDungeonCanvasEditor->RegisterModeToolbarIfUnregistered(GetModeName()))
	{
		InDungeonCanvasEditor->GetDungeonCanvasToolbarBuilder()->AddCanvasGraphModeToolbar(Toolbar);

		InDungeonCanvasEditor->GetToolbarBuilder()->AddCompileToolbar(Toolbar);
		InDungeonCanvasEditor->GetToolbarBuilder()->AddScriptingToolbar(Toolbar);
		// disabled: InDungeonCanvasEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(Toolbar);
	}
}

void FDungeonCanvasApplicationModeCanvasGraph::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) {
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());
	BP->PushTabFactories(TabFactories);
	
	FDungeonCanvasApplicationModeBase::RegisterTabFactories(InTabManager);
}

void FDungeonCanvasApplicationModeCanvasGraph::PreDeactivateMode() {
	FDungeonCanvasApplicationModeBase::PreDeactivateMode();
}

void FDungeonCanvasApplicationModeCanvasGraph::PostActivateMode() {
	FDungeonCanvasApplicationModeBase::PostActivateMode();
}

#undef LOCTEXT_NAMESPACE

