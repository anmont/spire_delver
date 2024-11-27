//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"

class FDungeonCanvasEditorCommands : public TCommands<FDungeonCanvasEditorCommands>
{
public:
	FDungeonCanvasEditorCommands();

	virtual void RegisterCommands() override;
public:
	TSharedPtr<FUICommandInfo> EditCanvasSettings;
	TSharedPtr<FUICommandInfo> EditDungeonSettings;
};

