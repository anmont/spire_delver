//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Editors/CanvasEditor/AppModes/DungeonCanvasApplicationModeBase.h"

#include "Core/Editors/CanvasEditor/AppModes/DungeonCanvasApplicationModes.h"
#include "Core/Editors/CanvasEditor/DungeonCanvasEditor.h"

FDungeonCanvasApplicationModeBase::FDungeonCanvasApplicationModeBase(TSharedPtr<FDungeonCanvasEditor> InDungeonCanvasEditor, FName InModeName)
	: FBlueprintEditorApplicationMode(InDungeonCanvasEditor, InModeName,
			DA::DungeonCanvas::Private::FDungeonCanvasApplicationModes::GetLocalizedMode, false, false)
	, BlueprintEditorWeakPtr(InDungeonCanvasEditor)
{
	
}

UDungeonCanvasBlueprint* FDungeonCanvasApplicationModeBase::GetBlueprint() const
{
	if (const TSharedPtr<FDungeonCanvasEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
	{
		return BlueprintEditor->GetDungeonCanvasBlueprint();
	}
	return nullptr;
}

TSharedPtr<FDungeonCanvasEditor> FDungeonCanvasApplicationModeBase::GetBlueprintEditor() const
{
	return BlueprintEditorWeakPtr.Pin();
}

