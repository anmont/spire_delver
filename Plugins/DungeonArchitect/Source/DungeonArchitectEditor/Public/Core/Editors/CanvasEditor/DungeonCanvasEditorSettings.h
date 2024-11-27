//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "DungeonCanvasEditorSettings.generated.h"

/**
 * Enumerates background for the texture editor view port.
 */
UENUM()
enum class EDungeonCanvasEditorBackgrounds : int8
{
	SolidColor UMETA(DisplayName="Solid Color"),
	Checkered UMETA(DisplayName="Checkered"),
	CheckeredFill UMETA(DisplayName="Checkered (Fill)")
};

UENUM()
enum class EDungeonCanvasEditorSampling : int8
{
	Default UMETA(DisplayName = "Default Sampling"),
	Point UMETA(DisplayName = "Nearest-Point Sampling"),
};

UENUM()
enum class EDungeonCanvasEditorZoomMode : uint8
{
	Custom    UMETA(DisplayName = "Specific Zoom Level"), // First so that any new modes added don't change serialized value
	Fit       UMETA(DisplayName = "Scale Down to Fit"),
	Fill      UMETA(DisplayName = "Scale to Fill"),
};

class UMaterialFunctionMaterialLayerBlend;
class UMaterialFunctionMaterialLayer;

USTRUCT()
struct DUNGEONARCHITECTEDITOR_API FDungeonCanvasEditorMaterialLayerPreset {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	FText Title;
	
	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	FText Tooltip;

	UPROPERTY(EditDefaultsOnly, Category="Dungeon Canvas")
	TSoftObjectPtr<UMaterialFunctionMaterialLayer> MaterialLayer;
	
	UPROPERTY(EditDefaultsOnly, Category="Dungeon Canvas")
	TSoftObjectPtr<UMaterialFunctionMaterialLayerBlend> MaterialBlend;
};

USTRUCT()
struct DUNGEONARCHITECTEDITOR_API FDungeonCanvasEditorBuilderDefaults {
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="Snap Grid Flow")
	TSoftObjectPtr<class USnapGridFlowAsset> SgfGraph;
	
	UPROPERTY(EditAnywhere, Category="Snap Grid Flow")
	TSoftObjectPtr<class USnapGridFlowModuleDatabase> SgfModDB;
	
	UPROPERTY(EditAnywhere, Category="Snap Map")
	TSoftObjectPtr<class USnapMapAsset> SnapMapGraph;

	UPROPERTY(EditAnywhere, Category="Snap Map")
	TSoftObjectPtr<class USnapMapModuleDatabase> SnapMapModDB;
	
	UPROPERTY(EditAnywhere, Category="Grid Flow")
	TSoftObjectPtr<class UGridFlowAsset> GridFlowGraph;

	UPROPERTY(EditAnywhere, Category="Cell Flow")
	TSoftObjectPtr<class UCellFlowAsset> CellFlowGraph;
};

UCLASS()
class DUNGEONARCHITECTEDITOR_API UDungeonCanvasEditorDefaults : public UDataAsset {
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	TSoftObjectPtr<UMaterialFunctionMaterialLayerBlend> DefaultLayerBlend;
	
	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	TArray<FDungeonCanvasEditorMaterialLayerPreset> MaterialLayerPresets;

	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	FDungeonCanvasEditorBuilderDefaults BuilderDefaultAssets;
};


UCLASS(config=EditorPerProjectUserSettings)
class DUNGEONARCHITECTEDITOR_API UDungeonCanvasEditorSettings : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	
	/** The type of background to draw in the texture editor view port. */
	UPROPERTY(config)
	EDungeonCanvasEditorBackgrounds Background;

	/** The texture sampling mode used to render textures in the texture editor view port. */
	UPROPERTY(config)
	EDungeonCanvasEditorSampling Sampling;

	/** Background and foreground color used by Texture preview view ports. */
	UPROPERTY(config, EditAnywhere, Category=Background)
	FColor BackgroundColor;

	/** The first color of the checkered background. */
	UPROPERTY(config, EditAnywhere, Category=Background)
	FColor CheckerColorOne;

	/** The second color of the checkered background. */
	UPROPERTY(config, EditAnywhere, Category=Background)
	FColor CheckerColorTwo;

	/** The size of the checkered background tiles. */
	UPROPERTY(config, EditAnywhere, Category=Background, meta=(ClampMin="2", ClampMax="4096"))
	int32 CheckerSize;
	
	/** Whether the texture should scale to fit or fill the view port, or use a custom zoom level. */
	UPROPERTY(config)
	EDungeonCanvasEditorZoomMode ZoomMode;

	/** Color to use for the texture border, if enabled. */
	UPROPERTY(config, EditAnywhere, Category=TextureBorder)
	FColor TextureBorderColor;

	/** If true, displays a border around the texture. */
	UPROPERTY(config)
	bool TextureBorderEnabled;
};

