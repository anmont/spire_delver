//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/DungeonModel.h"


FDungeonFloorSettings UDungeonModel::CreateFloorSettings(const UDungeonConfig* InConfig) const {
	return {};
}

FBox UDungeonModel::GetDungeonBounds() const {
	return DungeonLayout.Bounds;
}

