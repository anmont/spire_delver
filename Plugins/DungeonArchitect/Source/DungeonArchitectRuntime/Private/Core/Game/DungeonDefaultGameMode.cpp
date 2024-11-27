//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Game/DungeonDefaultGameMode.h"

#include "Core/Dungeon.h"
#include "Core/Game/DungeonBuildSystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

////////////////////////// ADungeonDefaultGameMode //////////////////////////

ADungeonDefaultGameMode::ADungeonDefaultGameMode() {
}

void ADungeonDefaultGameMode::BeginPlay() {
	Super::BeginPlay();

	UWorld* World = GetWorld();
	Dungeon = Cast<ADungeon>(UGameplayStatics::GetActorOfClass(World, ADungeon::StaticClass()));
	DungeonBuildSystem = Cast<ADungeonBuildSystem>(UGameplayStatics::GetActorOfClass(GetWorld(), ADungeonBuildSystem::StaticClass()));
}

bool ADungeonDefaultGameMode::ReadyToStartMatch_Implementation() {
	if (DungeonBuildSystem) {
		return DungeonBuildSystem->GetBuildSystemComponent()->CanStartMatch();
	}
	return Super::ReadyToStartMatch_Implementation();
}

AActor* ADungeonDefaultGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) {
	if (DungeonBuildSystem) {
		if (DungeonBuildSystem->GetBuildSystemComponent()->CanStartMatch()) {
			if (APlayerStart* PlayerStart = Cast<APlayerStart>(UGameplayStatics::GetActorOfClass(GetWorld(), APlayerStart::StaticClass()))) {
				return PlayerStart;
			}
		}
	}
	return Super::FindPlayerStart_Implementation(Player, IncomingName);
}

