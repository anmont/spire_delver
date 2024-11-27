//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Layout/DungeonLayoutData.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "DungeonCanvasBlueprintFunctionLib.generated.h"

struct FDungeonLayoutData;
class ADungeonCanvas;
class UCanvasRenderTarget2D;
struct FDungeonCanvasDrawContext;


UCLASS()
class DUNGEONARCHITECTRUNTIME_API UDungeonCanvasBlueprintFunctionLib : public UBlueprintFunctionLibrary {
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void GenerateSDF(UTexture* FillTexture, UTexture* BorderTexture, UTexture* DynamicOcclusionTexture, UCanvasRenderTarget2D* SDFTexture);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void GenerateVoronoiSdfEffect(UTexture* SDFTexture, UTexture* BorderTexture, UCanvasRenderTarget2D* TargetEffectTexture, float ScaleMin = 5, float ScaleMax = 20);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void UpdateDynamicOcclusions(const FDungeonCanvasDrawContext& DrawContext, UCanvasRenderTarget2D* DynamicOcclusionTexture, const FTransform& WorldBoundsTransform,
			const TArray<FDungeonCanvasOverlayIcon>& OverlayIcons);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void BlurTexture3x(UCanvasRenderTarget2D* SourceTexture, UCanvasRenderTarget2D* DestinationTexture);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void BlurTexture5x(UCanvasRenderTarget2D* SourceTexture, UCanvasRenderTarget2D* DestinationTexture);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void DungeonCanvasDrawMaterial(const FDungeonCanvasDrawContext& DrawContext, UMaterialInterface* Material, const FTransform& WorldBoundsTransform);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void DungeonCanvasDrawLayoutIcons(const FDungeonCanvasDrawContext& DrawContext, const FDungeonLayoutData& DungeonLayout, const TArray<FDungeonCanvasOverlayIcon>& OverlayIcons, float OpacityMultiplier = 1.0);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void DungeonCanvasDrawStairIcons(const FDungeonCanvasDrawContext& DrawContext, const FDungeonLayoutData& DungeonLayout, const FDungeonCanvasOverlayInternalIcon& StairIcon, float OpacityMultiplier = 1.0);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void DungeonCanvasDrawTrackedActorIcons(const FDungeonCanvasDrawContext& DrawContext, const TArray<FDungeonCanvasOverlayIcon>& OverlayIcons, float OpacityMultiplier = 1.0);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void SetCanvasMaterialScalarParameter(UMaterialInstanceDynamic* Material, FName ParamName, float Value);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void SetCanvasMaterialVectorParameter(UMaterialInstanceDynamic* Material, FName ParamName, FVector Value);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void SetCanvasMaterialColorParameter(UMaterialInstanceDynamic* Material, FName ParamName, FLinearColor Value);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void SetCanvasMaterialTextureParameter(UMaterialInstanceDynamic* Material, FName ParamName, UTexture* Value);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void SetCanvasMaterialWorldBounds(UMaterialInstanceDynamic* Material, const FTransform& WorldBoundsTransform);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	static void UpdateFogOfWarTexture(const FDungeonCanvasDrawContext& DrawContext, UTexture* SDFTexture, const FTransform& WorldBoundsTransform);

private:
	static void BeginFogOfWarUpdate(UCanvasRenderTarget2D* FogOfWarExploredTexture, UCanvasRenderTarget2D* FogOfWarVisibilityTexture);
	static void UpdateFogOfWarExplorer(UCanvasRenderTarget2D* FogOfWarExploredTexture, UCanvasRenderTarget2D* FogOfWarVisibilityTexture,
			UTexture* SDFTexture, const FTransform& WorldBoundsTransform, const FVector2D& LightSourceLocation,
			float LightRadius, float NumShadowSamples, int ShadowJitterDistance);

};

