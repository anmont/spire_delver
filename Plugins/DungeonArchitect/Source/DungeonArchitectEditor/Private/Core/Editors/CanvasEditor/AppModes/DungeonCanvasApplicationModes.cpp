//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/CanvasEditor/AppModes/DungeonCanvasApplicationModes.h"


const FName DA::DungeonCanvas::Private::FDungeonCanvasApplicationModes::CanvasGraphMode("CanvasGraphMode");


FText DA::DungeonCanvas::Private::FDungeonCanvasApplicationModes::GetLocalizedMode(const FName InMode)
{
	static TMap<FName, FText> LocModes;
	if (LocModes.Num() == 0)
	{
		LocModes.Add(CanvasGraphMode, NSLOCTEXT("DungeonCanvasBlueprintModes", "CanvasGraphMode", "Canvas Graph"));
	}

	check(InMode != NAME_None);
	const FText* OutDesc = LocModes.Find(InMode);
	check(OutDesc);

	return *OutDesc;
}


