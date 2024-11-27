//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Game/DungeonBuildSystem.h"

#include "Builders/SnapGridFlow/SnapGridFlowDungeon.h"
#include "Builders/SnapMap/SnapMapDungeonBuilder.h"
#include "Core/Dungeon.h"
#include "Frameworks/LevelStreaming/DungeonLevelStreamingModel.h"

#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

//////////////////////// UDungeonBuildSystemComponent ////////////////////////
UDungeonBuildSystemComponent::UDungeonBuildSystemComponent() {
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	SetIsReplicatedByDefault(true);
}

void UDungeonBuildSystemComponent::BeginPlay() {
	Super::BeginPlay();

	if (!Dungeon) {
		Dungeon = Cast<ADungeon>(UGameplayStatics::GetActorOfClass(GetWorld(), ADungeon::StaticClass()));
	}

	if (Dungeon && Dungeon->Builder) {
		// Listen to dungeon build events
		Dungeon->OnDungeonBuildComplete.AddDynamic(this, &UDungeonBuildSystemComponent::OnBuildComplete);

		// Check if we use level streaming
		bUsesLevelStreaming = Dungeon->Builder->IsA<USnapMapDungeonBuilder>() || Dungeon->Builder->IsA<USnapGridFlowBuilder>();
		if (bUsesLevelStreaming && Dungeon->LevelStreamingModel) {
			Dungeon->LevelStreamingModel->OnInitialChunksLoaded.AddDynamic(this, &UDungeonBuildSystemComponent::OnInitialChunksLoaded);
		}

		if (bAutoBuildOnPlay) {
			// TODO: Implement me
			// BuildDungeon();
		}
	}
}

void UDungeonBuildSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
}

void UDungeonBuildSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	
}

void UDungeonBuildSystemComponent::MulticastBuildDungeon_Implementation(int32 InSeed) {
	
}

void UDungeonBuildSystemComponent::ServerBuildDungeon_Implementation(int32 InSeed) {
	
}

bool UDungeonBuildSystemComponent::ServerBuildDungeon_Validate(int32 InSeed) {
	return true;
}

void UDungeonBuildSystemComponent::BuildDungeonImpl() {
	bIsDungeonReady = false;
	if (Dungeon) {
		if (bRandomizeSeedOnBuild) {
			Dungeon->RandomizeSeed();
		}

		Dungeon->BuildDungeon();
	}
}

bool UDungeonBuildSystemComponent::CanStartMatch_Implementation() {
	return bIsDungeonReady;
}

void UDungeonBuildSystemComponent::OnInitialChunksLoaded(ADungeon* InDungeon) {
	check(InDungeon == Dungeon);
	if (bUsesLevelStreaming) {
		ServerHandleDungeonReady();
	}
}

void UDungeonBuildSystemComponent::OnBuildComplete(ADungeon* InDungeon, bool bSuccess) {
	check(InDungeon == Dungeon);
	if (!bUsesLevelStreaming) {
		ServerHandleDungeonReady();
	}
}


void UDungeonBuildSystemComponent::ServerHandleDungeonReady() {
	bIsDungeonReady = true;
	
}

///////////////////////////// ADungeonBuildSystem /////////////////////////////
ADungeonBuildSystem::ADungeonBuildSystem() {
	bReplicates = true;
	bAlwaysRelevant = true;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>("Sprite");
	Billboard->SetupAttachment(RootComponent);

	BuildSystemComponent = CreateDefaultSubobject<UDungeonBuildSystemComponent>("BuildSystem");
}

void ADungeonBuildSystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADungeonBuildSystem, BuildSystemComponent);
}

