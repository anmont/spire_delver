//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Layout/DungeonLayoutData.h"
#include "Frameworks/Canvas/DungeonCanvasStructs.h"
#include "Frameworks/Canvas/DungeonCanvasViewport.h"

#include "Curves/CurveFloat.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "RenderGraphResources.h"
#include "DungeonCanvas.generated.h"

class UDungeonCanvasBlueprint;
class ADungeonCanvas;
class ADungeon;
class UCanvasRenderTarget2D;
struct FDungeonFloorSettings;
class UDungeonModel;
class UDungeonConfig;
class UDungeonEditorViewportProperties;
class USceneComponent;
class FRHIGPUBufferReadback;

enum class FDungeonCanvasHeightRangeType : uint8 {
	EntireDungeon,
	Floor,
	CustomRange
};

class DUNGEONARCHITECTRUNTIME_API FDungeonCanvasHeightRange {
public:
	void SetHeightRangeForSingleFloor(const FDungeonFloorSettings& InFloorSettings, int32 InFloorIndex);
	void SetHeightRangeForMultipleFloors(const FDungeonFloorSettings& InFloorSettings, const TArray<int32>& InFloorIndices);
	void SetHeightRangeAcrossAllFloors();
	void SetHeightRangeCustom(float InHeightRangeMin, float InHeightRangeMax);
	void SetBaseHeightOffset(float InBaseHeightOffset) { BaseHeightOffset = InBaseHeightOffset; }
	int32 GetFloorIndexAtHeight(const FDungeonFloorSettings& InFloorSettings, float HeightZ) const;
	bool InsideActiveFloorHeightRange(float HeightZ) const;
		
	FDungeonCanvasHeightRangeType GetRangeType() const { return RangeType; }
	float GetMinHeight() const { return HeightRangeMin; }
	float GetMaxHeight() const { return HeightRangeMax; }
	
private:
	void UpdateFloorRange(const FDungeonFloorSettings& InFloorSettings);

private:
	// User vars
	FDungeonCanvasHeightRangeType RangeType = FDungeonCanvasHeightRangeType::EntireDungeon;
	TArray<int> ActiveFloorIndices;

	// Calculated values
	float HeightRangeMin = -100000000;
	float HeightRangeMax = 100000000;
	float BaseHeightOffset{};
};


UCLASS(EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable, Abstract, HideDropdown)
class DUNGEONARCHITECTRUNTIME_API UDungeonCanvasActorIconFilter : public UObject {
	GENERATED_BODY()
public:
	virtual void ApplyFilter(const FDungeonCanvasViewportTransform& View, const TMap<FName, FDungeonCanvasOverlayIcon>& OverlayIconMap, const TArray<FName>& ItemTags, TObjectPtr<UObject>& ResourceObject,
		FVector2D& CanvasSize, FVector2D& CanvasLocation, float& ScreenSize, FLinearColor& Tint, float& Rotation) { }
};

UCLASS()
class DUNGEONARCHITECTRUNTIME_API UDungeonCanvasCircularFrameActorIconFilter : public UDungeonCanvasActorIconFilter {
	GENERATED_BODY()
public:
	virtual void ApplyFilter(const FDungeonCanvasViewportTransform& View, const TMap<FName, FDungeonCanvasOverlayIcon>& OverlayIconMap, const TArray<FName>& ItemTags, TObjectPtr<UObject>& ResourceObject,
		FVector2D& CanvasSize, FVector2D& CanvasLocation, float& ScreenSize, FLinearColor& Tint, float& Rotation) override;

public:
	UPROPERTY(EditAnywhere, Category="Dungeon Canvas", meta=(UIMin=0, UIMax=1))
	float BoundaryRadius = 1;

	/** The icon to show when the object is out of the circular bounds, e.g. an arrow of sorts */
	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	FName BoundaryEdgeIconName;

	/** How far should the object be from the edge before we remove the edge icon (we don't want to show arrows for far away objects) */
	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	float BoundaryEdgeIconMaxDisplayDistance = 0.15f;
	
	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	TArray<FName> TrackedIconTags;
	
	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	bool bEnableScaleBeyondBoundary = true;

	UPROPERTY(EditAnywhere, Category="Dungeon Canvas", meta=(EditCondition="bEnableScaleBeyondBoundary"))
	TSoftObjectPtr<UCurveFloat> ScaleCurve;
};


USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDungeonCanvasDrawSettings {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon Architect")
	bool bRotateToView = false;

	/** The base canvas rotation, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon Architect")
	float BaseCanvasRotation = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon Architect")
	bool bFogOfWarEnabled = false;

	/** start with fog of war map fully explored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon Architect")
	bool bFogOfWarFullyExplored = false;
	
	UPROPERTY(BlueprintReadWrite, Category="Dungeon Architect")
	UMaterialInstanceDynamic* FogOfWarMaterialInstanceOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, SimpleDisplay, Category = Advanced)
	TArray<UDungeonCanvasActorIconFilter*> OverlayActorIconFilters;
};

USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDungeonCanvasDrawContext {
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category="DungeonCanvas")
	ADungeonCanvas* DungeonCanvas = {};
	
	UPROPERTY(BlueprintReadOnly, Category="DungeonCanvas")
	UCanvas* Canvas = {};

	UPROPERTY(BlueprintReadOnly, Category="DungeonCanvas")
	FDungeonCanvasViewportTransform ViewTransform;

	UPROPERTY(BlueprintReadOnly, Category="DungeonCanvas")
	FDungeonCanvasDrawSettings DrawSettings;
	
	bool bEditorPreviewMode = false;
};

USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDungeonCanvasTrackedActorRegistryItem {
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Dungeon Canvas")
	TWeakObjectPtr<UDungeonCanvasTrackedObject> TrackedComponent;

	UPROPERTY()
	FGuid InstanceID;

	/* Is this item in the explored region of the fog of war. The value is between 0..1, 0 = black, 1 = visible */
	UPROPERTY()
	float FogOfWarExplored = 0;
	
	/* Is this item in the line of sight region of the fog of war. The value is between 0..1, 0 = hidden, 1 = visible */
	UPROPERTY()
	float FogOfWarLineOfSight = 0;
};

USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDungeonCanvasItemFogOfWarSettings {
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon Canvas")
	float LightRadius = 2000;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon Canvas")
	int NumShadowSamples = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon Canvas")
	float ShadowJitterDistance = 30;
};

UCLASS(Abstract, HideDropdown, EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable)
class DUNGEONARCHITECTRUNTIME_API UDungeonCanvasEffectBase : public UObject {
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Dungeon")
	void Initialize();
	virtual void Initialize_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Dungeon")
	void Draw();
	virtual void Draw_Implementation();
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Dungeon")
	void Tick(float DeltaSeconds);
	virtual void Tick_Implementation(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Dungeon")
	void InitCanvasMaterial(UMaterialInstanceDynamic* Material);
	virtual void InitCanvasMaterial_Implementation(UMaterialInstanceDynamic* Material);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Dungeon")
	void InitFogOfWarMaterial(UMaterialInstanceDynamic* Material);
	virtual void InitFogOfWarMaterial_Implementation(UMaterialInstanceDynamic* Material);

	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void SetTextures(UCanvasRenderTarget2D* TexLayoutFill, UCanvasRenderTarget2D* TexLayoutBorder, UCanvasRenderTarget2D* TexSDF, UCanvasRenderTarget2D* TexDynamicOcclusion);

	UPROPERTY(BlueprintReadOnly, Category="Dungeon")
	TWeakObjectPtr<UCanvasRenderTarget2D> LayoutFillTexture;
	
	UPROPERTY(BlueprintReadOnly, Category="Dungeon")
	TWeakObjectPtr<UCanvasRenderTarget2D> LayoutBorderTexture;
	
	UPROPERTY(BlueprintReadOnly, Category="Dungeon")
	TWeakObjectPtr<UCanvasRenderTarget2D> SDFTexture;
	
	UPROPERTY(BlueprintReadOnly, Category="Dungeon")
	TWeakObjectPtr<UCanvasRenderTarget2D> DynamicOcclusionTexture;
	
	UPROPERTY()
	UMaterialFunctionMaterialLayerInstance* MaterialLayerInstance;
};

UCLASS(BlueprintType, Blueprintable)
class DUNGEONARCHITECTRUNTIME_API UDungeonCanvasMaterialLayer : public UObject {
	GENERATED_BODY()
public:
	UDungeonCanvasMaterialLayer();
	
	UPROPERTY(EditAnywhere, Category="Material Layer")
	FText LayerName;

	UPROPERTY(EditAnywhere, Category="Material Layer")
	FText LayerDescription;

	UPROPERTY(EditAnywhere, Instanced, AdvancedDisplay, Category="Material Layer")
	UDungeonCanvasEffectBase* Effect;

	UPROPERTY(EditDefaultsOnly, Category="Material Functions")
	TSoftObjectPtr<UMaterialFunctionMaterialLayer> MaterialLayer;
	
	UPROPERTY(EditDefaultsOnly, Category="Material Functions")
	TSoftObjectPtr<UMaterialFunctionMaterialLayerBlend> MaterialBlend;

	UPROPERTY()
	bool bEnabled = true;
	
	UPROPERTY()
	int32 LayerIndex = INDEX_NONE;
};

UCLASS(Blueprintable, BlueprintType)
class DUNGEONARCHITECTRUNTIME_API ADungeonCanvas : public AActor {
	GENERATED_BODY()
public:
	ADungeonCanvas();

	//UObject interface
	virtual void PostInitializeComponents() override;
	//~ End UObject Interface

	// AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor Interface

public:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Settings")
	ADungeon* Dungeon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta = (UIMin = "0.0", UIMax = "100.0"))
	float LayoutDrawMarginPercent = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, SimpleDisplay, Category = "Settings", meta=(EditInline))
	TArray<UDungeonCanvasEffectBase*> Effects;
	
	UPROPERTY(BlueprintReadOnly, Category="DungeonCanvas")
	FDungeonLayoutData DungeonLayout;

	/* The items in the scene, like Tracked Overlay Actors, will have their fog of war data fetched from the GPU texture buffer. Control the frequency here */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Fog Of War")
	float FogOfWarItemStateUpdateFPS = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    TSoftObjectPtr<UMaterialInterface> MaterialTemplateCanvas;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    TSoftObjectPtr<UMaterialInterface> MaterialTemplateFogOfWar;
	
	/**
	 * If this instance is used for the editor viewport preview, DA sets this flag and lets the
	 * designer render the preview viewport differently, e.g. disable fog of war
	 */
	UPROPERTY(BlueprintReadOnly, Category="DungeonCanvas")
	bool bEditorPreviewMode = false;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="DungeonCanvas")
	const TArray<FDungeonCanvasTrackedActorRegistryItem>& GetTrackedOverlayActors() const { return TrackedOverlayActors; }

	UFUNCTION(BlueprintCallable, Category="DungeonCanvas")
	void AddTrackedOverlayActor(UDungeonCanvasTrackedObject* TrackedComponent);
	
private:
	/** The unique ID of this dungeon canvas */
	UPROPERTY()
	FGuid Guid;

	/** GetWorld calls can be expensive, we speed them up by   caching the last found world until it goes away. */
	mutable TWeakObjectPtr<UWorld> CachedWorldWeakPtr;

	UPROPERTY(Transient)
	TArray<UCanvasRenderTarget2D*> ManagedRenderTargets;

	UPROPERTY(Transient)
	TArray<FDungeonCanvasTrackedActorRegistryItem> TrackedOverlayActors;
	
public:
	virtual void Initialize(bool bClearCachedResources = true);

	UFUNCTION(BlueprintImplementableEvent, Category="DungeonCanvas", Meta=(DisplayName="Initialize"))
	void ReceiveInitialize();
	
	UFUNCTION(BlueprintCallable, Category="DungeonCanvas")
	void RequestInitialize();

	virtual void Draw(const FDungeonCanvasDrawContext& DrawContext);

	UFUNCTION(BlueprintImplementableEvent, Category="DungeonCanvas", Meta=(DisplayName="Draw"))
	void ReceiveDraw(const FDungeonCanvasDrawContext& DrawContext);

	UFUNCTION(BlueprintCallable, Category="DungeonCanvas")
	void RequestDraw(UCanvas* Canvas, int Width, int Height, FDungeonCanvasDrawSettings DrawSettings);

	UFUNCTION(BlueprintImplementableEvent, Category="DungeonCanvas", Meta=(DisplayName="Update"))
	void ReceiveUpdate(const FDungeonCanvasDrawContext& UpdateContext, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category="DungeonCanvas")
	void RequestUpdate(float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, Category="DungeonCanvas")
	virtual UCanvasRenderTarget2D* CreateManagedTexture(int Width, int Height, ETextureRenderTargetFormat Format, FLinearColor ClearColor = FLinearColor::Black);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	FTransform DrawDungeonLayout(UTextureRenderTarget2D* OutLayoutFill, UTextureRenderTarget2D* OutLayoutBorder);
	
	//UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	//void DrawDungeonLayoutCustomLayer(const FDungeonLayoutData& DungeonLayout, const FBox& DrawBounds, UTextureRenderTarget2D* OutCustomLayerTexture, FDungeonCanvasDrawLayoutSettings DrawSettings, bool bShouldClearColor = false, FLinearColor ClearColor = FLinearColor::Black);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	void SetCameraLocation(FVector WorldLocation);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	void SetCameraSize(FVector WorldSize);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	void SetPlayerCanvasRotation(FRotator CanvasRotation);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	void SetCameraWorldTransform(FTransform WorldTransform);

	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	void SetHeightRangeForSingleFloor(int32 FloorIndex);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DungeonCanvas")
	int32 GetFloorIndexAtHeight(float HeightZ) const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DungeonCanvas")
	bool InsideActiveFloorHeightRange(float HeightZ) const;
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	void SetHeightRangeForMultipleFloors(const TArray<int32>& InFloorIndices);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	void SetHeightRangeAcrossAllFloors();
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	void SetHeightRangeCustom(float InHeightRangeMin, float InHeightRangeMax);
	
	UFUNCTION(BlueprintCallable, Category = "DungeonCanvas")
	void AddFogOfWarExplorer(AActor* Actor, FDungeonCanvasItemFogOfWarSettings Settings);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DungeonCanvas")
	UCanvasRenderTarget2D* GetFogOfWarExploredTexture();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DungeonCanvas")
	UCanvasRenderTarget2D* GetFogOfWarVisibilityTexture();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DungeonCanvas")
	void SetupPlayer(APawn* NewPlayerPawn, FDungeonCanvasItemFogOfWarSettings FogOfWarSettings);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DungeonCanvas")
	void TickPlayer();
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="DungeonCanvas")
	UMaterialInterface* GetCanvasBaseMaterial() const;
	
	void SetCanvasEnabled(bool bValue) { bCanvasEnabled = bValue; }

	struct FFogOfWarItemEntry {
		/** The actor to track */
		TWeakObjectPtr<AActor> ActorPtr;
		
		/** The settings used to update the fog of war texture for this actor */
		FDungeonCanvasItemFogOfWarSettings Settings;
	};
	const TArray<FFogOfWarItemEntry>& GetFogOfWarExplorers() const { return FogOfWarExplorers; }
	
protected:
	UFUNCTION(BlueprintCallable, Category="DungeonCanvas")
	UMaterialInstanceDynamic* CreateMaterialInstance(UMaterialInterface* MaterialTemplate, UObject* Outer);
	
	UFUNCTION()
	void OnDungeonBuildComplete(ADungeon* InDungeon, bool bSuccess);

	UFUNCTION()
	void SyncFogOfWarItemState();
	
	void ReadbackFogOfWarItemState_RenderThread();
	
    FTimerHandle FogOfWarTimerHandle;
	TArray<FFogOfWarItemEntry> FogOfWarExplorers;
	bool bInitialized{};

	enum class EFogOfWarGPUReadbackStage {
		Pending,
		AwaitingRead,
		Reading,
		Complete
	};
	class FFogOfWarGPUReadbackState {
	public:
		TSharedPtr<FRHIGPUBufferReadback, ESPMode::ThreadSafe> ReadbackBuffer;
		TRefCountPtr<FRDGPooledBuffer> OutputBuffer;

		EFogOfWarGPUReadbackStage GetStage() {
			FScopeLock ScopeLock(&CriticalSection);
			return Stage;
		}
		
		void SetStage(EFogOfWarGPUReadbackStage InStage) {
			FScopeLock ScopeLock(&CriticalSection);
			Stage = InStage;
		}
		
		TArray<FGuid> GetUniqueActorIds() {
			FScopeLock ScopeLock(&CriticalSection);
			return UniqueActorIds;
		}
		
		void SetUniqueActorIds(const TArray<FGuid>& InActorIds) {
			FScopeLock ScopeLock(&CriticalSection);
			UniqueActorIds = InActorIds;
		}

		TArray<FVector2f> DownloadReadbackBuffer();

	private:
		FCriticalSection CriticalSection;
		EFogOfWarGPUReadbackStage Stage = EFogOfWarGPUReadbackStage::Pending;
		TArray<FGuid> UniqueActorIds;
	};
	FFogOfWarGPUReadbackState FogOfWarGPUReadbackState;
private:
	void DestroyManagedResources();

	UPROPERTY()
	USceneComponent* SceneRoot = nullptr;	
	
public:
	UPROPERTY()
	FDungeonCanvasViewportTransform ViewportTransform;
	
	UPROPERTY()
	FDungeonCanvasViewportTransform FullDungeonTransform;

	bool bCanvasEnabled = true;
	FDungeonCanvasHeightRange HeightRange;

	UPROPERTY()
	FRotator PlayerCanvasRotation = FRotator::ZeroRotator;

	UPROPERTY()
	FTransform DungeonTransform = FTransform::Identity;

	//UPROPERTY(VisibleAnywhere, Category="DungeonCanvas")
	//UDungeonCanvasBlueprint* DungeonCanvasAsset;
};

