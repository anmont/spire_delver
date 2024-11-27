//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Canvas/DungeonCanvasBlueprint.h"

#include "Frameworks/Canvas/DungeonCanvasBlueprintGeneratedClass.h"

#include "Materials/MaterialInstanceConstant.h"

UDungeonCanvasBlueprint::UDungeonCanvasBlueprint() {
	PreviewDungeonProperties = CreateDefaultSubobject<UDungeonCanvasEditorViewportProperties>("PreviewDungeonProperties");
	PreviewDungeonProperties->SetFlags(RF_DefaultSubObject | RF_Transactional);
}

#if WITH_EDITOR
UClass* UDungeonCanvasBlueprint::GetBlueprintClass() const {
	return UDungeonCanvasBlueprintGeneratedClass::StaticClass();
}
#endif // WITH_EDITOR

