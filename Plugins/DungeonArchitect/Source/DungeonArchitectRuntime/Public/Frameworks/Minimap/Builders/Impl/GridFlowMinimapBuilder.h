//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Frameworks/Minimap/Builders/DungeonMinimapBuilder.h"

class FGridFlowMinimapBuilder : public FDungeonMinimapBuilder {
public:
	virtual void BuildLayoutTexture(UWorld* InWorld, const FTransform& WorldToScreen, const FLayoutBuildSettings& InSettings, const FDungeonLayoutData& LayoutData, const UDungeonModel* InDungeonModel, UTexture*& OutMaskTexture) override;
};

