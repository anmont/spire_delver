//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"

class FDungeonCanvasEditor;
class FSceneViewport;
class SScrollBar;
class SViewport;
class FDungeonCanvasEditorViewportClient;

/**
 * Implements the dungeon canvas editor's view port.
 */
class SDungeonCanvasEditorViewport
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE(FOnDungeonRebuildEvent);
	
	SLATE_BEGIN_ARGS(SDungeonCanvasEditorViewport) { }
		SLATE_EVENT(FOnDungeonRebuildEvent, OnDungeonRebuild)
	SLATE_END_ARGS()

	/**
	 */
	void AddReferencedObjects( FReferenceCollector& Collector );

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<FDungeonCanvasEditor>& InToolkit );
	
	/**
	 * Modifies the checkerboard texture's data.
	 */
	void ModifyCheckerboardTextureColors( );


	/** Enable viewport rendering */
	void EnableRendering();

	/** Disable viewport rendering */
	void DisableRendering();

	FReply RandomizeDungeon() const;
	
	TSharedPtr<SViewport> GetViewportWidget( ) const;
	TSharedPtr<SScrollBar> GetVerticalScrollBar( ) const;
	TSharedPtr<SScrollBar> GetHorizontalScrollBar( ) const;
	TSharedPtr<FSceneViewport> GetViewport( ) const;
	TSharedPtr<FDungeonCanvasEditorViewportClient> GetViewportClient() const { return ViewportClient; }

public:

	// SWidget overrides

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

protected:

	/**
	 * Gets the displayed textures resolution as a string.
	 *
	 * @return Texture resolution string.
	 */
	FText GetDisplayedResolution() const;

private:

	// Callback for the horizontal scroll bar.
	void HandleHorizontalScrollBarScrolled( float InScrollOffsetFraction );

	// Callback for getting the visibility of the horizontal scroll bar.
	EVisibility HandleHorizontalScrollBarVisibility( ) const;

	// Callback for the vertical scroll bar.
	void HandleVerticalScrollBarScrolled( float InScrollOffsetFraction );

	// Callback for getting the visibility of the horizontal scroll bar.
	EVisibility HandleVerticalScrollBarVisibility( ) const;

	// Pointer back to the canvas editor tool that owns us.
	TWeakPtr<FDungeonCanvasEditor> ToolkitPtr;
	
	// Level viewport client.
	TSharedPtr<class FDungeonCanvasEditorViewportClient> ViewportClient;

	// Slate viewport for rendering and IO.
	TSharedPtr<FSceneViewport> Viewport;

	// Viewport widget.
	TSharedPtr<SViewport> ViewportWidget;

	// Vertical scrollbar.
	TSharedPtr<SScrollBar> CanvasViewportVerticalScrollBar;

	// Horizontal scrollbar.
	TSharedPtr<SScrollBar> CanvasViewportHorizontalScrollBar;

	TSharedPtr<SOverlay> ViewportOverlay;

	FOnDungeonRebuildEvent OnDungeonRebuild;
	
	// Is rendering currently enabled? (disabled when reimporting a texture)
	bool bIsRenderingEnabled = true;
};

