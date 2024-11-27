//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "DungeonBoundingShapes.generated.h"

UENUM()
enum class EDABoundsShapeType : uint8 {
	Polygon,
	Box,
	Circle
};

struct DUNGEONARCHITECTRUNTIME_API FDABoundsShapeConstants {
	static const FName TagDoNotRenderOnCanvas;
};

USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDABoundsShapeCircle {
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	float Height{};

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	FTransform Transform;

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	float Radius{};
	
	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	TArray<FName> Tags;
};

USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDABoundsShapeLine {
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	float Height = 0;

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	FTransform Transform;

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	FVector2D LineStart = FVector2D::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	FVector2D LineEnd = FVector2D::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	TArray<FName> Tags;
};


USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDABoundsShapeConvexPoly {
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	float Height{};
	
	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	FTransform Transform;

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	TArray<FVector2D> Points;

	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	TArray<FName> Tags;
};

USTRUCT(BlueprintType)
struct DUNGEONARCHITECTRUNTIME_API FDABoundsShapeList {
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	TArray<FDABoundsShapeConvexPoly> ConvexPolys;
	
	UPROPERTY(VisibleAnywhere, Category=DungeonArchitect)
	TArray<FDABoundsShapeCircle> Circles;

	FDABoundsShapeList TransformBy(const FTransform& InTransform) const;
	FORCEINLINE int32 GetTotalCustomShapes() const {
		// Texture shapes do not contribute to the bounds
		return ConvexPolys.Num() + Circles.Num();
	}
};


class DUNGEONARCHITECTRUNTIME_API FDABoundsShapeCollision {
public:
	static void ConvertBoxToConvexPoly(const FBox& InBox, FDABoundsShapeConvexPoly& OutPoly);
	static void ConvertBoxToConvexPoly(const FTransform& InTransform, const FVector& InExtent, FDABoundsShapeConvexPoly& OutPoly);
	static bool Intersects(const FDABoundsShapeConvexPoly& A, const FDABoundsShapeConvexPoly& B, float Tolerance);
	static bool Intersects(const FDABoundsShapeConvexPoly& A, const FDABoundsShapeCircle& B, float Tolerance);
	static bool Intersects(const FDABoundsShapeCircle& A, const FDABoundsShapeCircle& B, float Tolerance);
	static bool Intersects(const FDABoundsShapeList& A, const FDABoundsShapeList& B, float Tolerance);
	static bool Intersects(const FDABoundsShapeConvexPoly& Poly, const FDABoundsShapeLine& Line, float Tolerance);
	
	static bool Intersects(const FBox& A, const FDABoundsShapeConvexPoly& B, float Tolerance);
	static bool Intersects(const FBox& A, const FDABoundsShapeCircle& B, float Tolerance);
	static bool Intersects(const FBox& A, const FDABoundsShapeList& B, float Tolerance);

	static void TransformPoints(const TArray<FVector2D>& InPoints, const FTransform& InTransform, TArray<FVector2D>& OutTransformedPoints);
	
private:
	static void PopulatePolyProjectionNormals(const TArray<FVector2D>& InTransformedPoints, TSet<FVector2D>& Visited, TArray<FVector2D>& OutProjections);
	static void PopulateCircleProjectionNormals(const FVector2D& InTransformedCenter, const TArray<FVector2D>& InTransformedPolyPoints, TSet<FVector2D>& Visited, TArray<FVector2D>& OutProjections);
	
	FORCEINLINE static float GetOverlapsDistance(float MinA, float MaxA, float MinB, float MaxB) {
		return FMath::Min(MaxA, MaxB) - FMath::Max(MinA, MinB);
	}

	template<typename TA, typename TB>
	static float GetHeightOverlapDistance(const TA& A, const TB& B) {
		const float MinA = A.Transform.GetLocation().Z;
		const float MaxA = MinA + A.Height;
		const float MinB = B.Transform.GetLocation().Z;
		const float MaxB = MinB + B.Height;
		return GetOverlapsDistance(MinA, MaxA, MinB, MaxB);
	}
    
	template<typename TA, typename TB>
	static bool HeightOverlaps(const TA& A, const TB& B, float Tolerance) {
		return GetHeightOverlapDistance(A, B) - Tolerance > 0;
	}
	
	template<typename TB>
	static float GetBoxHeightOverlapDistance(const FBox& A, const TB& B) {
		const float MinA = A.Min.Z;
		const float MaxA = A.Max.Z;
		const float MinB = B.Transform.GetLocation().Z;
		const float MaxB = MinB + B.Height;
		return GetOverlapsDistance(MinA, MaxA, MinB, MaxB);
	}
	
	template<typename TB>
	static bool BoxHeightOverlaps(const FBox& A, const TB& B, float Tolerance) {
		return GetBoxHeightOverlapDistance(A, B) - Tolerance > 0;
	}
};

