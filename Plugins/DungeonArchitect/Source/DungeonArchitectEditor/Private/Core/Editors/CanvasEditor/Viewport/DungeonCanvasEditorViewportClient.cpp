//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/CanvasEditor/Viewport/DungeonCanvasEditorViewportClient.h"

#include "Core/Editors/CanvasEditor/DungeonCanvasEditor.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditorSettings.h"
#include "Core/Editors/CanvasEditor/Viewport/SDungeonCanvasEditorViewport.h"
#include "Frameworks/Canvas/DungeonCanvas.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "ImageUtils.h"
#include "Slate/SceneViewport.h"

/* FDungeonCanvasEditorViewportClient structs
 *****************************************************************************/

FDungeonCanvasEditorViewportClient::FDungeonCanvasEditorViewportClient( TWeakPtr<FDungeonCanvasEditor> InCanvasEditor, TWeakPtr<SDungeonCanvasEditorViewport> InCanvasEditorViewport )
	: CanvasEditorPtr(InCanvasEditor)
	, CanvasEditorViewportPtr(InCanvasEditorViewport)
	, CheckerboardTexture(nullptr)
{
	check(CanvasEditorPtr.IsValid() && CanvasEditorViewportPtr.IsValid());

	ModifyCheckerboardTextureColors();

	static const FName CanvasName(TEXT("DungeonCanvasViewportObject"));
	CanvasObject = NewObject<UCanvas>(GetTransientPackage(), CanvasName);
}


FDungeonCanvasEditorViewportClient::~FDungeonCanvasEditorViewportClient( )
{
	DestroyCheckerboardTexture();
}


/* FViewportClient interface
 *****************************************************************************/

void FDungeonCanvasEditorViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	if (!CanvasEditorPtr.IsValid())
	{
		return;
	}

	TSharedPtr<FDungeonCanvasEditor> CanvasEditorPinned = CanvasEditorPtr.Pin();
	const UDungeonCanvasEditorSettings& Settings = *GetDefault<UDungeonCanvasEditorSettings>();
	
	FVector2D Ratio = FVector2D(GetViewportHorizontalScrollBarRatio(), GetViewportVerticalScrollBarRatio());
	FVector2D ViewportSize = FVector2D(CanvasEditorViewportPtr.Pin()->GetViewport()->GetSizeXY());
	FVector2D ScrollBarPos = GetViewportScrollBarPositions();
	int32 BorderSize = Settings.TextureBorderEnabled ? 1 : 0;
	float YOffset = static_cast<float>((Ratio.Y > 1.0) ? ((ViewportSize.Y - (ViewportSize.Y / Ratio.Y)) * 0.5) : 0);
	int32 YPos = (int32)FMath::Clamp(FMath::RoundToInt(YOffset - ScrollBarPos.Y + BorderSize), TNumericLimits<int32>::Min(), TNumericLimits<int32>::Max());
	float XOffset = static_cast<float>((Ratio.X > 1.0) ? ((ViewportSize.X - (ViewportSize.X / Ratio.X)) * 0.5) : 0);
	int32 XPos = (int32)FMath::Clamp(FMath::RoundToInt(XOffset - ScrollBarPos.X + BorderSize), TNumericLimits<int32>::Min(), TNumericLimits<int32>::Max());
	
	UpdateScrollBars();

	Canvas->Clear( Settings.BackgroundColor );
	
	// Figure out the size we need
	int32 Width, Height, Depth, ArraySize;
	CanvasEditorPinned->CalculateTextureDimensions(Width, Height, Depth, ArraySize, false);
	
	if (CanvasObject) {
		CanvasObject->Init(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, nullptr, Canvas);
		if (ADungeonCanvas* CanvasInstance = CanvasEditorPinned->GetInstance(); IsValid(CanvasInstance)) {
			FDungeonCanvasViewportTransform ViewTransform = CanvasInstance->ViewportTransform;
			const FVector CanvasSize = FVector(Width, Height, 0);
			const FVector CanvasBaseLocation = FVector(XPos, YPos, 0) + CanvasSize * 0.5f;
			const FTransform LocalToCanvas(FRotator::ZeroRotator, CanvasBaseLocation, CanvasSize);
			ViewTransform.SetLocalToCanvas(LocalToCanvas);

			// Draw the canvas
			const FDungeonCanvasDrawContext DrawContext{ CanvasInstance, CanvasObject, ViewTransform, {}, true };
			CanvasInstance->Draw(DrawContext);
		}
	}

	// Draw a white border around the texture to show its extents
	if (Settings.TextureBorderEnabled)
	{
		float ScaledBorderSize = ((float)BorderSize - 1) * 0.5f;
		FCanvasBoxItem BoxItem(FVector2D((float)XPos - ScaledBorderSize, (float)YPos - ScaledBorderSize), FVector2D(Width + BorderSize, Height + BorderSize));
		BoxItem.LineThickness = (float)BorderSize;
		BoxItem.SetColor( Settings.TextureBorderColor );
		Canvas->DrawItem( BoxItem );
	}
}


bool FDungeonCanvasEditorViewportClient::InputKey(const FInputKeyEventArgs& InEventArgs)
{
	if (InEventArgs.Event == IE_Pressed)
	{
		if (InEventArgs.Key == EKeys::MouseScrollUp)
		{
			CanvasEditorPtr.Pin()->ZoomIn();

			return true;
		}
		else if (InEventArgs.Key == EKeys::MouseScrollDown)
		{
			CanvasEditorPtr.Pin()->ZoomOut();

			return true;
		}
	}
	return false;
}

bool FDungeonCanvasEditorViewportClient::InputAxis(FViewport* Viewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if (Key == EKeys::MouseX || Key == EKeys::MouseY)
	{
		TSharedPtr<FDungeonCanvasEditor> TextureEditorPinned = CanvasEditorPtr.Pin();
		if (ShouldUseMousePanning(Viewport))
		{
			TSharedPtr<SDungeonCanvasEditorViewport> EditorViewport = CanvasEditorViewportPtr.Pin();

			int32 Width, Height, Depth, ArraySize;
			TextureEditorPinned->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

			if (Key == EKeys::MouseY)
			{
				float VDistFromBottom = EditorViewport->GetVerticalScrollBar()->DistanceFromBottom();
				float VRatio = GetViewportVerticalScrollBarRatio();
				float localDelta = (Delta / static_cast<float>(Height));
				EditorViewport->GetVerticalScrollBar()->SetState(FMath::Clamp((1.f - VDistFromBottom - VRatio) + localDelta, 0.0f, 1.0f - VRatio), VRatio);
			}
			else
			{
				float HDistFromBottom = EditorViewport->GetHorizontalScrollBar()->DistanceFromBottom();
				float HRatio = GetViewportHorizontalScrollBarRatio();
				float localDelta = (Delta / static_cast<float>(Width)) * -1.f; // delta needs to be inversed
				EditorViewport->GetHorizontalScrollBar()->SetState(FMath::Clamp((1.f - HDistFromBottom - HRatio) + localDelta, 0.0f, 1.0f - HRatio), HRatio);
			}
		}
		return true;
	}

	return false;
}

bool FDungeonCanvasEditorViewportClient::ShouldUseMousePanning(FViewport* Viewport) const
{
	if (Viewport->KeyState(EKeys::RightMouseButton))
	{
		TSharedPtr<SDungeonCanvasEditorViewport> EditorViewport = CanvasEditorViewportPtr.Pin();
		return EditorViewport.IsValid() && EditorViewport->GetVerticalScrollBar().IsValid() && EditorViewport->GetHorizontalScrollBar().IsValid();
	}

	return false;
}

EMouseCursor::Type FDungeonCanvasEditorViewportClient::GetCursor(FViewport* Viewport, int32 X, int32 Y)
{
	return ShouldUseMousePanning(Viewport) ? EMouseCursor::GrabHandClosed : EMouseCursor::Default;
}

bool FDungeonCanvasEditorViewportClient::InputGesture(FViewport* Viewport, EGestureEvent GestureType, const FVector2D& GestureDelta, bool bIsDirectionInvertedFromDevice)
{
	const bool LeftMouseButtonDown = Viewport->KeyState(EKeys::LeftMouseButton);
	const bool RightMouseButtonDown = Viewport->KeyState(EKeys::RightMouseButton);

	if (GestureType == EGestureEvent::Scroll && !LeftMouseButtonDown && !RightMouseButtonDown)
	{
		double CurrentZoom = CanvasEditorPtr.Pin()->GetCustomZoomLevel();
		CanvasEditorPtr.Pin()->SetCustomZoomLevel(CurrentZoom + GestureDelta.Y * 0.01);
		return true;
	}

	return false;
}

UWorld* FDungeonCanvasEditorViewportClient::GetWorld() const {
	TSharedPtr<FDungeonCanvasEditor> CanvasEditor = CanvasEditorPtr.Pin();
	if (CanvasEditor.IsValid()) {
		if (const ADungeonCanvas* DungeonCanvasInstance = CanvasEditor->GetInstance()) {
			return DungeonCanvasInstance->GetWorld();
		}
	}
	return nullptr;
}


void FDungeonCanvasEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CheckerboardTexture);
	Collector.AddReferencedObject(CanvasObject);
}


void FDungeonCanvasEditorViewportClient::ModifyCheckerboardTextureColors()
{
	DestroyCheckerboardTexture();

	const UDungeonCanvasEditorSettings& Settings = *GetDefault<UDungeonCanvasEditorSettings>();
	CheckerboardTexture = FImageUtils::CreateCheckerboardTexture(Settings.CheckerColorOne, Settings.CheckerColorTwo, Settings.CheckerSize);
}


FText FDungeonCanvasEditorViewportClient::GetDisplayedResolution() const
{
	// Zero is the default size 
	int32 Height, Width, Depth, ArraySize;
	CanvasEditorPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, false);

	FNumberFormattingOptions Options;
	Options.UseGrouping = false;

	if (Depth > 0)
	{
		return FText::Format(NSLOCTEXT("TextureEditor", "DisplayedResolutionThreeDimension", "Displayed: {0}x{1}x{2}"), FText::AsNumber(Width, &Options), FText::AsNumber(Height, &Options), FText::AsNumber(Depth, &Options));
	}
	else
	{
		return FText::Format(NSLOCTEXT("TextureEditor", "DisplayedResolutionTwoDimension", "Displayed: {0}x{1}"), FText::AsNumber(Width, &Options), FText::AsNumber(Height, &Options));
	}
}


float FDungeonCanvasEditorViewportClient::GetViewportVerticalScrollBarRatio() const
{
	int32 Height = 1;
	int32 Width = 1;
	float WidgetHeight = 1.0f;
	if (CanvasEditorViewportPtr.Pin()->GetVerticalScrollBar().IsValid())
	{
		int32 Depth, ArraySize;
		CanvasEditorPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

		WidgetHeight = (float)(CanvasEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().Y);
	}

	return WidgetHeight / (float)Height;
}


float FDungeonCanvasEditorViewportClient::GetViewportHorizontalScrollBarRatio() const
{
	int32 Width = 1;
	int32 Height = 1;
	float WidgetWidth = 1.0f;
	if (CanvasEditorViewportPtr.Pin()->GetHorizontalScrollBar().IsValid())
	{
		int32 Depth, ArraySize;
		CanvasEditorPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

		WidgetWidth = (float)(CanvasEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().X);
	}

	return WidgetWidth / (float)Width;
}


void FDungeonCanvasEditorViewportClient::UpdateScrollBars()
{
	TSharedPtr<SDungeonCanvasEditorViewport> Viewport = CanvasEditorViewportPtr.Pin();

	if (!Viewport.IsValid() || !Viewport->GetVerticalScrollBar().IsValid() || !Viewport->GetHorizontalScrollBar().IsValid())
	{
		return;
	}

	float VRatio = GetViewportVerticalScrollBarRatio();
	float HRatio = GetViewportHorizontalScrollBarRatio();
	float VDistFromTop = Viewport->GetVerticalScrollBar()->DistanceFromTop();
	float VDistFromBottom = Viewport->GetVerticalScrollBar()->DistanceFromBottom();
	float HDistFromTop = Viewport->GetHorizontalScrollBar()->DistanceFromTop();
	float HDistFromBottom = Viewport->GetHorizontalScrollBar()->DistanceFromBottom();

	if (VRatio < 1.0f)
	{
		if (VDistFromBottom < 1.0f)
		{
			Viewport->GetVerticalScrollBar()->SetState(FMath::Clamp((1.0f + VDistFromTop - VDistFromBottom - VRatio) * 0.5f, 0.0f, 1.0f - VRatio), VRatio);
		}
		else
		{
			Viewport->GetVerticalScrollBar()->SetState(0.0f, VRatio);
		}
	}

	if (HRatio < 1.0f)
	{
		if (HDistFromBottom < 1.0f)
		{
			Viewport->GetHorizontalScrollBar()->SetState(FMath::Clamp((1.0f + HDistFromTop - HDistFromBottom - HRatio) * 0.5f, 0.0f, 1.0f - HRatio), HRatio);
		}
		else
		{
			Viewport->GetHorizontalScrollBar()->SetState(0.0f, HRatio);
		}
	}
}


FVector2D FDungeonCanvasEditorViewportClient::GetViewportScrollBarPositions() const
{
	FVector2D Positions = FVector2D::ZeroVector;
	if (CanvasEditorViewportPtr.Pin()->GetVerticalScrollBar().IsValid() && CanvasEditorViewportPtr.Pin()->GetHorizontalScrollBar().IsValid())
	{
		int32 Width, Height, Depth, ArraySize;
		float VRatio = GetViewportVerticalScrollBarRatio();
		float HRatio = GetViewportHorizontalScrollBarRatio();
		float VDistFromTop = CanvasEditorViewportPtr.Pin()->GetVerticalScrollBar()->DistanceFromTop();
		float VDistFromBottom = CanvasEditorViewportPtr.Pin()->GetVerticalScrollBar()->DistanceFromBottom();
		float HDistFromTop = CanvasEditorViewportPtr.Pin()->GetHorizontalScrollBar()->DistanceFromTop();
		float HDistFromBottom = CanvasEditorViewportPtr.Pin()->GetHorizontalScrollBar()->DistanceFromBottom();
	
		CanvasEditorPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

		if ((CanvasEditorViewportPtr.Pin()->GetVerticalScrollBar()->GetVisibility() == EVisibility::Visible) && VDistFromBottom < 1.0f)
		{
			Positions.Y = FMath::Clamp((1.0f + VDistFromTop - VDistFromBottom - VRatio) * 0.5f, 0.0f, 1.0f - VRatio) * (float)Height;
		}
		else
		{
			Positions.Y = 0.0f;
		}

		if ((CanvasEditorViewportPtr.Pin()->GetHorizontalScrollBar()->GetVisibility() == EVisibility::Visible) && HDistFromBottom < 1.0f)
		{
			Positions.X = FMath::Clamp((1.0f + HDistFromTop - HDistFromBottom - HRatio) * 0.5f, 0.0f, 1.0f - HRatio) * (float)Width;
		}
		else
		{
			Positions.X = 0.0f;
		}
	}

	return Positions;
}

void FDungeonCanvasEditorViewportClient::DestroyCheckerboardTexture()
{
	if (CheckerboardTexture)
	{
		if (CheckerboardTexture->GetResource())
		{
			CheckerboardTexture->ReleaseResource();
		}
		CheckerboardTexture->MarkAsGarbage();
		CheckerboardTexture = NULL;
	}
}

