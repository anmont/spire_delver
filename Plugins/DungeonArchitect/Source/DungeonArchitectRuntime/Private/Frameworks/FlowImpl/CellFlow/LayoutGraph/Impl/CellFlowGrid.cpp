//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/FlowImpl/CellFlow/LayoutGraph/Impl/CellFlowGrid.h"


FVector2d UDAFlowCellLeafNodeGrid::GetCenter() const {
	return FVector2d(Location.X, Location.Y) + FVector2d(Size.X, Size.Y) * 0.5;
}

float UDAFlowCellLeafNodeGrid::GetArea() const {
	return Size.X * Size.Y;
}



