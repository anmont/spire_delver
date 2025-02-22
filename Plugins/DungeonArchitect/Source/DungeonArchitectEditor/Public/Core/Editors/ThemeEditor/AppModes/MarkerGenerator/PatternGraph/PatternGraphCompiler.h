//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"

class UMarkerGenPattern;
class UMarkerGenPatternRule;

class FMGPatternGraphCompiler {
public:
	static void Compile(UMarkerGenPattern* InPattern);
	static void Compile(const UMarkerGenPatternRule* InRule);
};

