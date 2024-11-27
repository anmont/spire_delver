//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NonLatentCurveTimeline.generated.h"

class UCurveFloat;

USTRUCT(Blueprintable)
struct DUNGEONARCHITECTRUNTIME_API FNonLatentCurveTimeline {
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Dungeon Architect")
	UCurveFloat* Curve = nullptr;

	UPROPERTY()
	float Time = 0;
};

UCLASS(Blueprintable)
class DUNGEONARCHITECTRUNTIME_API UNonLatentCurveTimelineBlueprintFunctionLib : public UBlueprintFunctionLibrary {
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category="Dungeon Architect", meta=(WorldContext="WorldContextObject", DefaultToSelf="WorldContextObject"))
	static void TickForward(UObject* WorldContextObject, UPARAM(ref) FNonLatentCurveTimeline& Timeline);
	
	UFUNCTION(BlueprintCallable, Category="Dungeon Architect", meta=(WorldContext="WorldContextObject", DefaultToSelf="WorldContextObject"))
	static void TickBackward(UObject* WorldContextObject, UPARAM(ref) FNonLatentCurveTimeline& Timeline);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Dungeon Architect")
	static float GetValue(const FNonLatentCurveTimeline& Timeline);

private:
	static void TickImpl(FNonLatentCurveTimeline& Timeline, float DeltaSeconds);
	static float GetDeltaSeconds(UObject* WorldContextObject);
	
};

