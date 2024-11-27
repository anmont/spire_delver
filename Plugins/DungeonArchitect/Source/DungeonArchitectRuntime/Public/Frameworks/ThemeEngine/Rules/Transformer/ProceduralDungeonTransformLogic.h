//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "ProceduralDungeonTransformLogic.generated.h"

/**
 *
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable, HideDropDown)
class DUNGEONARCHITECTRUNTIME_API UProceduralDungeonTransformLogic : public UObject {
	GENERATED_BODY()
public:
	virtual FTransform Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) { return FTransform::Identity; }
};



/**
 *
 */
UCLASS(EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable)
class DUNGEONARCHITECTRUNTIME_API URandomTranslateProcTransformLogic : public UProceduralDungeonTransformLogic {
	GENERATED_BODY()
public:
	virtual FTransform Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) override;

	UPROPERTY(EditAnywhere, Category="Random Transform")
	float MoveAlongX{};
	
	UPROPERTY(EditAnywhere, Category="Random Transform")
	float MoveAlongY{};
	
	UPROPERTY(EditAnywhere, Category="Random Transform")
	float MoveAlongZ{};
};


/**
 *
 */
UCLASS(EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable)
class DUNGEONARCHITECTRUNTIME_API URandomJitterProcTransformLogic : public UProceduralDungeonTransformLogic {
	GENERATED_BODY()
public:
	virtual FTransform Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) override;

	UPROPERTY(EditAnywhere, Category=" Jitter")
	float JitterDistance{3};
	
	UPROPERTY(EditAnywhere, Category=" Jitter")
	bool JitterAlongXY{true};
	
	UPROPERTY(EditAnywhere, Category="Jitter")
	bool JitterAlongZ{};
};


/**
 * Rotate randomly along the Z axis
 */
UCLASS(EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable)
class DUNGEONARCHITECTRUNTIME_API URandomRotateZProcTransformLogic : public UProceduralDungeonTransformLogic {
	GENERATED_BODY()
public:
	virtual FTransform Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) override;
};

/**
 * Rotate randomly along the Z axis in 90 degrees steps
 */
UCLASS(EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable)
class DUNGEONARCHITECTRUNTIME_API URandomRotateZ90ProcTransformLogic : public UProceduralDungeonTransformLogic {
	GENERATED_BODY()
public:
	virtual FTransform Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) override;
};


/**
 * Rotate randomly along the Z axis
 */
UCLASS(EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable)
class DUNGEONARCHITECTRUNTIME_API URandomRotateProcTransformLogic : public UProceduralDungeonTransformLogic {
	GENERATED_BODY()
public:
	virtual FTransform Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) override;

	UPROPERTY(EditAnywhere, Category=" Jitter")
	float RotateXAngle{360};
	
	UPROPERTY(EditAnywhere, Category=" Jitter")
	float RotateYAngle{360};
	
	UPROPERTY(EditAnywhere, Category=" Jitter")
	float RotateZAngle{360};
};

