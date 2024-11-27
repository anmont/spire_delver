//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"

class SDungeonCanvasEditorViewport;
class FDungeonCanvasEditor;

class FDungeonCanvasEditorViewportClient 
	: public FViewportClient
	, public FGCObject
{
public:
	/** Constructor */
	FDungeonCanvasEditorViewportClient(TWeakPtr<FDungeonCanvasEditor> InCanvasEditor, TWeakPtr<SDungeonCanvasEditorViewport> InCanvasEditorViewport);
	~FDungeonCanvasEditorViewportClient();

	/** FViewportClient interface */
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	virtual bool InputKey(const FInputKeyEventArgs& InEventArgs) override;
	virtual bool InputAxis(FViewport* Viewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) override;
	virtual bool InputGesture(FViewport* Viewport, EGestureEvent GestureType, const FVector2D& GestureDelta, bool bIsDirectionInvertedFromDevice) override;
	virtual UWorld* GetWorld() const override;
	virtual EMouseCursor::Type GetCursor(FViewport* Viewport, int32 X, int32 Y) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FTextureEditorViewportClient");
	}

	/** Modifies the checkerboard texture's data */
	void ModifyCheckerboardTextureColors();

	/** Returns a string representation of the currently displayed textures resolution */
	FText GetDisplayedResolution() const;

	/** Returns the ratio of the size of the canvas texture to the size of the viewport */
	float GetViewportVerticalScrollBarRatio() const;
	float GetViewportHorizontalScrollBarRatio() const;
	
private:
	/** Updates the states of the scrollbars */
	void UpdateScrollBars();

	/** Returns the positions of the scrollbars relative to the canvas textures */
	FVector2D GetViewportScrollBarPositions() const;

	/** Destroy the checkerboard texture if one exists */
	void DestroyCheckerboardTexture();

	/** TRUE if right clicking and dragging for panning a texture 2D */
	bool ShouldUseMousePanning(FViewport* Viewport) const;

private:
	/** Pointer back to the canvas editor tool that owns us */
	TWeakPtr<FDungeonCanvasEditor> CanvasEditorPtr;

	/** Pointer back to the canvas viewport control that owns us */
	TWeakPtr<SDungeonCanvasEditorViewport> CanvasEditorViewportPtr;

	/** Checkerboard texture */
	TObjectPtr<UTexture2D> CheckerboardTexture;

	TObjectPtr<UCanvas> CanvasObject;
};

