//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"

struct FDungeonCanvasEditorConstants {
	// MaxZoom and MinZoom should both be powers of two
	static const double MaxZoom;
	static const double MinZoom;
	
	// ZoomFactor is multiplicative such that an integer number of steps will give a power of two zoom (50% or 200%)
	static const int ZoomFactorLogSteps;
	static const double ZoomFactor;

	// Specifies the maximum allowed exposure bias.
	static const int32 MaxExposure;

	// Specifies the minimum allowed exposure bias.
	static const int32 MinExposure;
};

