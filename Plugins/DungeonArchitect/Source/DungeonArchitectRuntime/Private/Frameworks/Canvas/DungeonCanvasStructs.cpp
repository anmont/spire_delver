//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Canvas/DungeonCanvasStructs.h"

#include "Frameworks/Canvas/DungeonCanvas.h"

#include "EngineUtils.h"

void UDungeonCanvasTrackedObject::BeginPlay() {
	Super::BeginPlay();

	// TODO: Create and use a world subsystem to cache the canvas actor. Searching through it using an actor iterator is a linear op and costly for complex maps
	// Find the canvas component add the overlay actor
	for (TActorIterator<ADungeonCanvas> It(GetWorld()); It; ++It) {
		if (ADungeonCanvas* DungeonCanvas = *It) {
			DungeonCanvas->AddTrackedOverlayActor(this);
		}
	}
}

