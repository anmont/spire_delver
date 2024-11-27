//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Utils/NonLatentCurveTimeline.h"

#include "Curves/CurveFloat.h"
#include "Engine/Engine.h"

void UNonLatentCurveTimelineBlueprintFunctionLib::TickForward(UObject* WorldContextObject, FNonLatentCurveTimeline& Timeline) {
	const float DeltaSeconds = GetDeltaSeconds(WorldContextObject);
	TickImpl(Timeline, DeltaSeconds);
}

void UNonLatentCurveTimelineBlueprintFunctionLib::TickBackward(UObject* WorldContextObject, FNonLatentCurveTimeline& Timeline) {
	const float DeltaSeconds = GetDeltaSeconds(WorldContextObject);
	TickImpl(Timeline, -DeltaSeconds);
}

float UNonLatentCurveTimelineBlueprintFunctionLib::GetValue(const FNonLatentCurveTimeline& Timeline) {
	if (Timeline.Curve) {
		float MinTime{}, MaxTime{};
		Timeline.Curve->GetTimeRange(MinTime, MaxTime);
		const float ClampedTime = FMath::Clamp(Timeline.Time, MinTime, MaxTime);
		return Timeline.Curve->GetFloatValue(ClampedTime);
	}
	else {
		return 0;
	}
}

void UNonLatentCurveTimelineBlueprintFunctionLib::TickImpl(FNonLatentCurveTimeline& Timeline, float DeltaSeconds) {
	if (Timeline.Curve) {
		float MinTime{}, MaxTime{};
		Timeline.Curve->GetTimeRange(MinTime, MaxTime);
		Timeline.Time += DeltaSeconds;
		Timeline.Time = FMath::Clamp(Timeline.Time, MinTime, MaxTime);
	}
}

float UNonLatentCurveTimelineBlueprintFunctionLib::GetDeltaSeconds(UObject* WorldContextObject) {
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)) {
		return World->GetDeltaSeconds();
	}
	return 0;
}

