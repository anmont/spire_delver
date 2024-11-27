//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Editors/CanvasEditor/AppModes/DungeonCanvasApplicationModeBase.h"

class FDungeonCanvasEditor;

class FDungeonCanvasApplicationModeCanvasGraph : public FDungeonCanvasApplicationModeBase
{
public:
	FDungeonCanvasApplicationModeCanvasGraph(TSharedPtr<FDungeonCanvasEditor> InDungeonCanvasEditor);

	//~ Begin FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;
	
	//~ End FApplicationMode interface

private:
	FWorkflowAllowedTabSet TabFactories;
};

