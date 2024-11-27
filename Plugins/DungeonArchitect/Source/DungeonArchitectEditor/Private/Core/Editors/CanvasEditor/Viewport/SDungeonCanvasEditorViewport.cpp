//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/CanvasEditor/Viewport/SDungeonCanvasEditorViewport.h"

#include "Core/Editors/CanvasEditor/Viewport/DungeonCanvasEditorViewportClient.h"

#include "SEditorViewportToolBarButton.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"

#define LOCTEXT_NAMESPACE "SDungeonCanvasEditorViewport"

/* SDungeonCanvasEditorViewport interface
 *****************************************************************************/

void SDungeonCanvasEditorViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	ViewportClient->AddReferencedObjects(Collector);
}

namespace DungeonCanvasEditorViewport {
	static void CaptureThumbnail() {
		
	}
}

void SDungeonCanvasEditorViewport::Construct( const FArguments& InArgs, const TSharedRef<FDungeonCanvasEditor>& InToolkit ) {
	OnDungeonRebuild = InArgs._OnDungeonRebuild;
	bIsRenderingEnabled = true;
	ToolkitPtr = InToolkit;

	const FMargin ToolbarSlotPadding(4.0f, 4.0f);
	
	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)
			// viewport canvas
			+ SOverlay::Slot()
			.Padding(5.0f, 0.0f)
			[
				SAssignNew(ViewportWidget, SViewport)
				.EnableGammaCorrection(true)
				.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
				.ShowEffectWhenDisabled(false)
				.EnableBlending(true)
				[
					SAssignNew(ViewportOverlay, SOverlay)
					+SOverlay::Slot()
					.VAlign(VAlign_Top)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ToolbarSlotPadding)
						[
							SNew(SEditorViewportToolBarButton)
							.Cursor(EMouseCursor::Default)
							.ButtonType(EUserInterfaceActionType::Button)
							.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
							.OnClicked(this, &SDungeonCanvasEditorViewport::RandomizeDungeon)
							.ToolTipText(LOCTEXT("DungeonRandomize_Tooltip", "Randomize the preview dungeon"))
							.Content()
							[
								SNew(SBox)
								.Padding(4)
								[
									SNew(STextBlock)
									.Font(FAppStyle::GetFontStyle("EditorViewportToolBar.Font"))
									.Text(LOCTEXT("RandomizeDungeon", "Randomize Dungeon"))
									//.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
									.ColorAndOpacity(FLinearColor::Black)
								]
							]
						]
					]
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			[
				// vertical scroll bar
				SAssignNew(CanvasViewportVerticalScrollBar, SScrollBar)
				.Visibility(this, &SDungeonCanvasEditorViewport::HandleVerticalScrollBarVisibility)
				.OnUserScrolled(this, &SDungeonCanvasEditorViewport::HandleVerticalScrollBarScrolled)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// horizontal scrollbar
			SAssignNew(CanvasViewportHorizontalScrollBar, SScrollBar)
				.Orientation( Orient_Horizontal )
				.Visibility(this, &SDungeonCanvasEditorViewport::HandleHorizontalScrollBarVisibility)
				.OnUserScrolled(this, &SDungeonCanvasEditorViewport::HandleHorizontalScrollBarScrolled)
		]

	];
	
	ViewportClient = MakeShareable(new FDungeonCanvasEditorViewportClient(ToolkitPtr, SharedThis(this)));

	Viewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), ViewportWidget));

	// The viewport widget needs an interface so it knows what should render
	ViewportWidget->SetViewportInterface( Viewport.ToSharedRef() );
}

void SDungeonCanvasEditorViewport::ModifyCheckerboardTextureColors( )
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->ModifyCheckerboardTextureColors();
	}
}

void SDungeonCanvasEditorViewport::EnableRendering()
{
	bIsRenderingEnabled = true;
}

void SDungeonCanvasEditorViewport::DisableRendering()
{
	bIsRenderingEnabled = false;
}

FReply SDungeonCanvasEditorViewport::RandomizeDungeon() const {
	if (OnDungeonRebuild.IsBound()) {
		OnDungeonRebuild.Execute();
	}
	
	return FReply::Handled();
}

TSharedPtr<FSceneViewport> SDungeonCanvasEditorViewport::GetViewport( ) const
{
	return Viewport;
}

TSharedPtr<SViewport> SDungeonCanvasEditorViewport::GetViewportWidget( ) const
{
	return ViewportWidget;
}

TSharedPtr<SScrollBar> SDungeonCanvasEditorViewport::GetVerticalScrollBar( ) const
{
	return CanvasViewportVerticalScrollBar;
}

TSharedPtr<SScrollBar> SDungeonCanvasEditorViewport::GetHorizontalScrollBar( ) const
{
	return CanvasViewportHorizontalScrollBar;
}


/* SWidget overrides
 *****************************************************************************/

void SDungeonCanvasEditorViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bIsRenderingEnabled)
	{
		Viewport->Invalidate();
	}
}


/* SDungeonCanvasEditorViewport implementation
 *****************************************************************************/

FText SDungeonCanvasEditorViewport::GetDisplayedResolution( ) const
{
	return ViewportClient->GetDisplayedResolution();
}


/* SDungeonCanvasEditorViewport event handlers
 *****************************************************************************/

void SDungeonCanvasEditorViewport::HandleHorizontalScrollBarScrolled( float InScrollOffsetFraction )
{
	const float Ratio = ViewportClient->GetViewportHorizontalScrollBarRatio();
	const float MaxOffset = (Ratio < 1.0f) ? 1.0f - Ratio : 0.0f;
	InScrollOffsetFraction = FMath::Clamp(InScrollOffsetFraction, 0.0f, MaxOffset);

	CanvasViewportHorizontalScrollBar->SetState(InScrollOffsetFraction, Ratio);
}

EVisibility SDungeonCanvasEditorViewport::HandleHorizontalScrollBarVisibility( ) const
{
	if (ViewportClient->GetViewportHorizontalScrollBarRatio() < 1.0f)
	{
		return EVisibility::Visible;
	}
	
	return EVisibility::Collapsed;
}

void SDungeonCanvasEditorViewport::HandleVerticalScrollBarScrolled( float InScrollOffsetFraction )
{
	const float Ratio = ViewportClient->GetViewportVerticalScrollBarRatio();
	const float MaxOffset = (Ratio < 1.0f) ? 1.0f - Ratio : 0.0f;
	InScrollOffsetFraction = FMath::Clamp(InScrollOffsetFraction, 0.0f, MaxOffset);

	CanvasViewportVerticalScrollBar->SetState(InScrollOffsetFraction, Ratio);
}

EVisibility SDungeonCanvasEditorViewport::HandleVerticalScrollBarVisibility( ) const
{
	if (ViewportClient->GetViewportVerticalScrollBarRatio() < 1.0f)
	{
		return EVisibility::Visible;
	}
	
	return EVisibility::Collapsed;
}


#undef LOCTEXT_NAMESPACE

