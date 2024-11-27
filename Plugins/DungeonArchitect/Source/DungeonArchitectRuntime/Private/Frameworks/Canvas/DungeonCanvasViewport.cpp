//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Canvas/DungeonCanvasViewport.h"


void FDungeonCanvasViewportTransform::SetLocalToWorld(const FTransform& InTransform) {
	LocalToWorld = InTransform;
	UpdateWorldAspectRatio();
	RecalculateTransforms();
	bFocusInitialized = true;		// TODO: get rid of this
}

void FDungeonCanvasViewportTransform::SetLocalToCanvas(const FTransform& InTransform) {
	LocalToCanvas = InTransform;
	UpdateWorldAspectRatio();
	RecalculateTransforms();
}

void FDungeonCanvasViewportTransform::FocusOnCanvas(float CanvasWidth, float CanvasHeight) {
	const FVector CanvasSize(CanvasWidth, CanvasHeight, 0);
	SetLocalToCanvas(FTransform(GetLocalToCanvas().GetRotation(), CanvasSize * 0.5, CanvasSize));
}

void FDungeonCanvasViewportTransform::UpdateWorldAspectRatio() {
	const FVector CanvasSize = LocalToCanvas.GetScale3D();
	FVector WorldSize = LocalToWorld.GetScale3D();
	const float CanvasAspectRatio = CanvasSize.X / CanvasSize.Y;
	const float WorldAspectRatio = WorldSize.X / WorldSize.Y;

	if (CanvasAspectRatio != WorldAspectRatio) {
		if (CanvasAspectRatio > WorldAspectRatio) {
			// Adjust width to match aspect ratio
			WorldSize.X = WorldSize.Y * CanvasAspectRatio;
		} else {
			// Adjust height to match aspect ratio
			WorldSize.Y = WorldSize.X / CanvasAspectRatio;
		}
	}
	LocalToWorld.SetScale3D(WorldSize);
}

FVector2D FDungeonCanvasViewportTransform::WorldToCanvasLocation(const FVector& InWorldLocation) const {
	return FVector2D(WorldToCanvas.TransformPosition(InWorldLocation));
}

FVector FDungeonCanvasViewportTransform::CanvasToWorldLocation(const FVector2D& InCanvasLocation) const {
	return CanvasToWorld.TransformPosition(FVector(InCanvasLocation, 0));
}

void FDungeonCanvasViewportTransform::RecalculateTransforms() {
	const FTransform WorldToLocal = LocalToWorld.Inverse();
	WorldToCanvas = WorldToLocal * LocalToCanvas;
	CanvasToWorld = WorldToCanvas.Inverse();
}

