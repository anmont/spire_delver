//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/CanvasEditor/DungeonCanvasEditorConstants.h"


const double FDungeonCanvasEditorConstants::MaxZoom = 16.0;
const double FDungeonCanvasEditorConstants::MinZoom = 1.0/64;
const int FDungeonCanvasEditorConstants::ZoomFactorLogSteps = 8;
const double FDungeonCanvasEditorConstants::ZoomFactor = FMath::Pow(2.0,1.0/ZoomFactorLogSteps);
const int32 FDungeonCanvasEditorConstants::MaxExposure = 10;
const int32 FDungeonCanvasEditorConstants::MinExposure = -10;

