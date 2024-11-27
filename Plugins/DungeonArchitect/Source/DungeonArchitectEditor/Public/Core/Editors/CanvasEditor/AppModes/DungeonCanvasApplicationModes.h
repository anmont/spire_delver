//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"

namespace DA::DungeonCanvas::Private
{
	/**
	 * This struct contains the IDs as well as the localized text of each of the render grid editor application modes.
	 */
	struct FDungeonCanvasApplicationModes
	{
	public:
		/** Constant for the default mode. */
		static const FName CanvasGraphMode;

	public:
		/** Returns the localized text for the given render grid editor application mode. */
		static FText GetLocalizedMode(const FName InMode);

	private:
		FDungeonCanvasApplicationModes() = default;
	};
}

