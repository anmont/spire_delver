//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "DungeonCanvasRoomShapeTexture.generated.h"

class UTexture;
class UTexture2D;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EDABoundsShapeTextureBlendMode : uint8 {
	Add,
	Multiply,
	Opaque
};


USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDungeonCanvasRoomShapeTexture {
	GENERATED_BODY()

	// The scale is applied on a 1m quad.  Apply the scale accordingly
	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	FTransform Transform;

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	UTexture2D* TextureMask = nullptr;

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	bool bRenderOnCustomLayer = false;
	
	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	FName CustomRenderLayerName = "MyLayer";

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	EDABoundsShapeTextureBlendMode BlendMode = EDABoundsShapeTextureBlendMode::Add;
	
	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	TArray<FName> Tags;

	static const FVector LocalQuadPoints[];
	static const FVector2D QuadUV[];
};

USTRUCT()
struct DUNGEONARCHITECTRUNTIME_API FDungeonCanvasRoomShapeTextureList {
	GENERATED_BODY()
	
	/** Texture shapes do not contribute to the bounds */
	UPROPERTY(EditAnywhere, Category=DungeonArchitect)
	TArray<FDungeonCanvasRoomShapeTexture> TextureShapes;

	FDungeonCanvasRoomShapeTextureList TransformBy(const FTransform& InTransform) const;
};

UCLASS(Blueprintable, hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), editinlinenew, meta=(BlueprintSpawnableComponent))
class DUNGEONARCHITECTRUNTIME_API UDungeonCanvasRoomShapeTextureComponent : public UPrimitiveComponent {
	GENERATED_BODY()
public:
	UDungeonCanvasRoomShapeTextureComponent();
	UTexture* GetOverlayTexture() const;
	
	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	TSoftObjectPtr<UTexture2D> OverlayTexture{};
	
	UPROPERTY(EditAnywhere, Category="Dungeon Canvas")
	TSoftObjectPtr<UMaterialInterface> OverlayMaterialTemplate;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Canvas")
	float Opacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Canvas")
	bool bVisualizeTexture = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Canvas")
	bool bEditorShowOnlyWhenSelected = true;

	/**
	 * By default, the texture will be rendered in the layout mask for generating the shape of the layout
	 * You can render your own RGB textures in a separate Render texture and use that in your materials
	 * If this is what you need, then enable this flag and choose a layer name.   Then call
	 * the render layout function in the canvas blueprint with this name and supply a texture
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Canvas")
	bool bRenderOnCustomLayer = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Canvas", meta=(EditCondition="bRenderOnCustomLayer"))
	FName CustomRenderLayerName = "MyLayer";
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	
private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> OverlayMaterial{};

	UPROPERTY()
	TObjectPtr<UTexture> WhiteTexture;

public:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool IsEditorOnly() const override { return true; }
	
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;
	TObjectPtr<UMaterialInstanceDynamic> GetOverlayMaterial() const;

	//~ Begin USceneComponent Interface
	virtual void InitializeComponent() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface

private:
	void RecreateOverlayMaterial();
};


UCLASS(hidecategories=(Input), ConversionRoot, ComponentWrapperClass)
class DUNGEONARCHITECTRUNTIME_API ADungeonCanvasRoomShapeTextureActor : public AActor {
	GENERATED_BODY()

public:
	ADungeonCanvasRoomShapeTextureActor();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Canvas", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDungeonCanvasRoomShapeTextureComponent> OverlayTextureComponent;


#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UBillboardComponent* Billboard;
#endif // WITH_EDITORONLY_DATA
};

