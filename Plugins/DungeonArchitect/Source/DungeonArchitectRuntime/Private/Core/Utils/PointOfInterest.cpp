//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Utils/PointOfInterest.h"

#include "Components/BillboardComponent.h"

ADungeonPointOfInterestActor::ADungeonPointOfInterestActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PointOfInterestComponent = ObjectInitializer.CreateDefaultSubobject<UDungeonPointOfInterestComponent>(this, "PointOfInterest");
	RootComponent = PointOfInterestComponent;

#if WITH_EDITORONLY_DATA
	Billboard = ObjectInitializer.CreateDefaultSubobject<UBillboardComponent>(this, "Billboard");
	Billboard->SetupAttachment(RootComponent);
#endif
}

FDungeonPointOfInterest UDungeonPointOfInterestComponent::GetPointOfInterest() const {
	FDungeonPointOfInterest Result;
	Result.Id = Id;
	Result.Caption = Caption;
	Result.Transform = GetComponentTransform();
	return Result;
}

