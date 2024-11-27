//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/CanvasEditor/DungeonCanvasEditorSettings.h"


UDungeonCanvasEditorSettings::UDungeonCanvasEditorSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, Background(EDungeonCanvasEditorBackgrounds::Checkered)
	, Sampling(EDungeonCanvasEditorSampling::Default)
	, BackgroundColor(FColor::FromHex("242424FF"))
	, CheckerColorOne(FColor(128, 128, 128))
	, CheckerColorTwo(FColor(64, 64, 64))
	, CheckerSize(16)
	, ZoomMode(EDungeonCanvasEditorZoomMode::Fit)
	, TextureBorderColor(FColor::White)
	, TextureBorderEnabled(true)
{ }

