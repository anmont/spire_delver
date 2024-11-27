//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Canvas/DungeonCanvasBlueprintGeneratedClass.h"

#include "Materials/MaterialInstanceConstant.h"

UDungeonCanvasBlueprintGeneratedClass::UDungeonCanvasBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

uint8* UDungeonCanvasBlueprintGeneratedClass::GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const {
	if (!IsInGameThread()) {
		// we cant use the persistent frame if we are executing in parallel (as we could potentially thunk to BP)
		return nullptr;
	}
	return Super::GetPersistentUberGraphFrame(Obj, FuncToCheck);
}

