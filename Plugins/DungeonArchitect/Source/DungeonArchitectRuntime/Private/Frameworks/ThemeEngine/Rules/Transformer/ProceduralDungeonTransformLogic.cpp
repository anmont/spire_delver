//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/ThemeEngine/Rules/Transformer/ProceduralDungeonTransformLogic.h"


FTransform URandomTranslateProcTransformLogic::Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) {
	const FVector Translation {
		(InRandom.FRand() * 2 - 1) * MoveAlongX,
		(InRandom.FRand() * 2 - 1) * MoveAlongY,
		(InRandom.FRand() * 2 - 1) * MoveAlongZ
	};

	return FTransform(Translation);
}

FTransform URandomJitterProcTransformLogic::Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) {
	const float DistanceXY = JitterAlongXY ? JitterDistance : 0;
	const float DistanceZ = JitterAlongZ ? JitterDistance : 0;
	const FVector Translation {
		(InRandom.FRand() * 2 - 1) * DistanceXY,
		(InRandom.FRand() * 2 - 1) * DistanceXY,
		(InRandom.FRand() * 2 - 1) * DistanceZ
	};

	return FTransform(Translation);
}

FTransform URandomRotateZProcTransformLogic::Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) {
	const float Angle = InRandom.FRand() * PI * 2;
	return FTransform(FQuat(FVector::UpVector, Angle));
}

FTransform URandomRotateZ90ProcTransformLogic::Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) {
	const float Angle = (PI * 0.5f) * InRandom.RandRange(0, 3);
	return FTransform(FQuat(FVector::UpVector, Angle));
}

FTransform URandomRotateProcTransformLogic::Execute(const FRandomStream& InRandom, const FTransform& InMarkerTransform) {
	FRotator Rotation = FRotator::MakeFromEuler(FVector(
		(InRandom.FRand() - 0.5f) * RotateXAngle,
		(InRandom.FRand() - 0.5f) * RotateYAngle,
		(InRandom.FRand() - 0.5f) * RotateZAngle
	));
	return Super::Execute(InRandom, InMarkerTransform);
}

