//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Core/Utils/CommonStructs.h"

#include "Components/SceneComponent.h"
#include "DungeonCanvasStructs.generated.h"

//////////////////////// Object Tracking ////////////////////////

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class DUNGEONARCHITECTRUNTIME_API UDungeonCanvasTrackedObject : public USceneComponent {
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	FName IconName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	bool bOrientToRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	bool bOccludesFogOfWar = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	bool bHideWhenOutOfSight = false;

	/** Higher numbers show up on top when overlapped with other icons. E.g. if you want your player icon to be drawn on top of everything else, bump this number up (e.g. 1000) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	int32 ZOrder = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas", meta=(EditCondition="bOccludesFogOfWar"))
	EDungeonArchitectImageChannel FogOfWarOcclusionChannel = EDungeonArchitectImageChannel::Alpha;

public:
	virtual void BeginPlay() override;

};


UENUM(BlueprintType)
enum class EDungeonCanvasIconCoordinateSystem : uint8 {
	Pixels = 0 UMETA(DisplayName = "Pixels"),
	WorldCoordinates UMETA(DisplayName = "World Coordinates"),
};

USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDungeonCanvasOverlayInternalIcon {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas", meta=(AllowPrivateAccess="true", DisplayThumbnail="true", DisplayName="Image", AllowedClasses="/Script/Engine.Texture,/Script/Engine.MaterialInterface,/Script/Engine.SlateTextureAtlasInterface", DisallowedClasses = "/Script/MediaAssets.MediaTexture"))
	TSoftObjectPtr<UObject> ResourceObject;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	FLinearColor Tint = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	float RotationOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	float Scale = 1.0f;
	
	/** The base rotation will be ignored.   Enable this if you want the icon to always face a certain angle.  E.g. When the whole map rotates when the player looks around,  if you have an up or down arrow in a lift room, you don't this arrow icon to rotate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	bool bAbsoluteRotation = false;
};

USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDungeonCanvasOverlayIcon {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas", meta=(AllowPrivateAccess="true", DisplayThumbnail="true", DisplayName="Image", AllowedClasses="/Script/Engine.Texture,/Script/Engine.MaterialInterface,/Script/Engine.SlateTextureAtlasInterface", DisallowedClasses = "/Script/MediaAssets.MediaTexture"))
	TObjectPtr<UObject> ResourceObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	float ScreenSize = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	EDungeonCanvasIconCoordinateSystem ScreenSizeType = EDungeonCanvasIconCoordinateSystem::Pixels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	float AspectRatio = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	float RotationOffset = 0.0f;

	/** The base rotation will be ignored.   Enable this if you want the icon to always face a certain angle.  E.g. When the whole map rotates when the player looks around,  if you have an up or down arrow in a lift room, you don't this arrow icon to rotate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	bool bAbsoluteRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Canvas")
	TArray<FName> Tags;
};

