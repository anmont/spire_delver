//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "ActorFactories/ActorFactoryBlueprint.h"
#include "DungeonCanvasBlueprintFactory.generated.h"

class ADungeonCanvas;
class UDungeonCanvasMaterialLayer;
class UDungeonCanvasBlueprint;

UCLASS()
class DUNGEONARCHITECTEDITOR_API UDungeonCanvasBlueprintFactory : public UFactory {
	GENERATED_UCLASS_BODY()
public:
	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual const TArray<FText>& GetMenuCategorySubMenus() const override;
	virtual bool ShouldShowInNewMenu() const override { return true; }
	// End of UFactory interface

	/** The parent class of the created blueprint. */
	UPROPERTY()
	TSubclassOf<ADungeonCanvas> ParentClass;

	UPROPERTY()
	TWeakObjectPtr<const UDungeonCanvasBlueprint> CanvasBlueprintTemplate;
};


UCLASS()
class DUNGEONARCHITECTEDITOR_API UDungeonCanvasActorFactory : public UActorFactoryBlueprint {
	GENERATED_UCLASS_BODY()

	// UActorFactory interface
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	// End of UActorFactory interface
};

