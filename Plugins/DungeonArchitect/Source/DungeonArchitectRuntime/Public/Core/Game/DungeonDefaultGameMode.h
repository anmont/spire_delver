//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "DungeonDefaultGameMode.generated.h"

class ADungeonBuildSystem;
class ADungeon;

UCLASS(Blueprintable, BlueprintType)
class ADungeonDefaultGameMode : public AGameMode {
	GENERATED_BODY()
public:
	ADungeonDefaultGameMode();
	
	virtual void BeginPlay() override;
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;

private:
	UPROPERTY()
	TObjectPtr<ADungeon> Dungeon;
	
	UPROPERTY()
	TObjectPtr<ADungeonBuildSystem> DungeonBuildSystem;
};

