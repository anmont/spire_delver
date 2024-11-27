//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Utils/DungeonEditorViewportProperties.h"

#include "Engine/Blueprint.h"
#include "DungeonCanvasBlueprint.generated.h"

class ADungeonCanvas;
class UDungeonEditorViewportProperties;
class UDungeonCanvasMaterialLayer;
class UMaterialInstanceConstant;


UCLASS()
class DUNGEONARCHITECTRUNTIME_API UDungeonCanvasEditorViewportProperties : public UDungeonEditorViewportProperties {
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder")
	bool bRandomizeDungeonOnBuild = true;
};

UCLASS(BlueprintType, Meta=(IgnoreClassThumbnail, DontUseGenericSpawnObject="true"))
class DUNGEONARCHITECTRUNTIME_API UDungeonCanvasBlueprint : public UBlueprint {
	GENERATED_BODY()
public:
	UDungeonCanvasBlueprint();

	//~ Begin UBlueprint Interface
#if WITH_EDITOR
	virtual bool AlwaysCompileOnLoad() const override { return true; }
	virtual UClass* GetBlueprintClass() const override;
#endif // WITH_EDITOR
	//~ End UBlueprint Interface

public:
	UPROPERTY()
	UDungeonCanvasEditorViewportProperties* PreviewDungeonProperties;

	UPROPERTY()
	TArray<UDungeonCanvasMaterialLayer*> MaterialLayers;

	UPROPERTY()
	UMaterialInstanceConstant* MaterialInstance = nullptr;
};

