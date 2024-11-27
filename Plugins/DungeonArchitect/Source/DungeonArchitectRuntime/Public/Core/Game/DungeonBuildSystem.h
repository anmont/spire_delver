//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "DungeonBuildSystem.generated.h"

class ADungeon;


UCLASS(Blueprintable, BlueprintType)
class UDungeonBuildSystemComponent : public UActorComponent {
	GENERATED_BODY()
public:
	UDungeonBuildSystemComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	
	/** Manually invoke the build system on the server */
	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = Dungeon)
	void ServerBuildDungeon(int32 InSeed);

	/** Multicast function to build the dungeon on all clients */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBuildDungeon(int32 InSeed);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent , Category="Dungeon")
	bool CanStartMatch();
	
private:
	UFUNCTION()
	void OnInitialChunksLoaded(ADungeon* InDungeon);
	
	UFUNCTION()
	void OnBuildComplete(ADungeon* InDungeon, bool bSuccess);

	void BuildDungeonImpl();
	void ServerHandleDungeonReady();
	
public:
	/** The dungeon actor to build. Leave it blank to automatically pick the first Dungeon actor it finds on the screen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Dungeon)
	TObjectPtr<ADungeon> Dungeon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Dungeon)
	bool bAutoBuildOnPlay = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Dungeon)
	bool bRandomizeSeedOnBuild = true;

private:
	UPROPERTY()
	bool bIsDungeonReady = false;

	bool bUsesLevelStreaming = false;

	int32 Seed{};
};


UCLASS(Blueprintable, BlueprintType, ComponentWrapperClass, meta=(ChildCanTick))
class ADungeonBuildSystem : public AActor {
	GENERATED_BODY()
public:
	ADungeonBuildSystem();
	
	TObjectPtr<UDungeonBuildSystemComponent> GetBuildSystemComponent() const { return BuildSystemComponent; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Category = Dungeon, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UDungeonBuildSystemComponent> BuildSystemComponent;
};

