//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/Package.h"
#include "DungeonCanvasBlueprintGeneratedClass.generated.h"

class UMaterialInterface;
class UMaterialInstanceConstant;
class UDungeonCanvasMaterialLayer;

UCLASS()
class DUNGEONARCHITECTRUNTIME_API UDungeonCanvasBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()
public:
	//~ Begin UBlueprintGeneratedClass interface
	virtual uint8* GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const override;
	//~ End UBlueprintGeneratedClass interface
	
	/**
	* Gets UDungeonCanvasBlueprintGeneratedClass from class hierarchy
	* @return UDungeonCanvasBlueprintGeneratedClass or NULL
	*/
	FORCEINLINE static UDungeonCanvasBlueprintGeneratedClass* GetDungeonCanvasGeneratedClass(UClass* InClass)
	{
		UDungeonCanvasBlueprintGeneratedClass* ScriptClass = nullptr;
		for (UClass* MyClass = InClass; MyClass && !ScriptClass; MyClass = MyClass->GetSuperClass()) {
			ScriptClass = Cast<UDungeonCanvasBlueprintGeneratedClass>(MyClass);
		}
		return ScriptClass;
	}

	UPROPERTY()
	UMaterialInstanceConstant* MaterialInstance;
	
	UPROPERTY()
	TArray<UDungeonCanvasMaterialLayer*> MaterialLayers;
};

