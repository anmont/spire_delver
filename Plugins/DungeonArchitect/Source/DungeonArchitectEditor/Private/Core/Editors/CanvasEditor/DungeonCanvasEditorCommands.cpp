//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/CanvasEditor/DungeonCanvasEditorCommands.h"


#define LOCTEXT_NAMESPACE "DungeonCanvasEditorCommands"

FDungeonCanvasEditorCommands::FDungeonCanvasEditorCommands()
	: TCommands<FDungeonCanvasEditorCommands>(
		TEXT("DungeonCanvasEditor"),
		NSLOCTEXT("Contexts", "DungeonCanvasEditor", "Dungeon Canvas Editor"),
		NAME_None,
		FAppStyle::GetAppStyleSetName()) {
	
}

void FDungeonCanvasEditorCommands::RegisterCommands() {
	UI_COMMAND(EditCanvasSettings, "Canvas Settings", "Edit Canvas Settings", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(EditDungeonSettings, "Preview Dungeon Settings", "Edit the Preview Dungeon Settings", EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE

