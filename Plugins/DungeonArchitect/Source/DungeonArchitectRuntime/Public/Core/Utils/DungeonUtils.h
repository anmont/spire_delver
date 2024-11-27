//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "EngineUtils.h"

class FDungeonUtils {
public:
	template<typename TActor>
	static void DestroyManagedActor(const UWorld* InWorld, const FGuid& InDungeonId) {
		for (TActorIterator<TActor> It(InWorld); It; ++It) {
			TActor* ManagedActor = *It;
			if (ManagedActor && ManagedActor->DungeonID == InDungeonId) {
				ManagedActor->Destroy();
			}
		}
	}
};

