//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "BlueprintEditorModes.h"

class UDungeonCanvasBlueprint;
class FDungeonCanvasEditor;

class FDungeonCanvasApplicationModeBase : public FBlueprintEditorApplicationMode
{
public:
	FDungeonCanvasApplicationModeBase(TSharedPtr<FDungeonCanvasEditor> InDungeonCanvasEditor, FName InModeName);

protected:
	/** Returns the DungeonCanvasBlueprint of the editor that was given to the constructor. */
	UDungeonCanvasBlueprint* GetBlueprint() const;

	/** Returns the editor that was given to the constructor. */
	TSharedPtr<FDungeonCanvasEditor> GetBlueprintEditor() const;

protected:
	/** The editor that was given to the constructor. */
	TWeakPtr<FDungeonCanvasEditor> BlueprintEditorWeakPtr;

	/** Set of spawnable tabs in the mode. */
	FWorkflowAllowedTabSet TabFactories;
};

