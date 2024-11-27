//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Utils/DungeonBoundingShapes.h"


const FName FDABoundsShapeConstants::TagDoNotRenderOnCanvas = TEXT("CANVAS_IGNORE");

FDABoundsShapeList FDABoundsShapeList::TransformBy(const FTransform& InTransform) const {
	FDABoundsShapeList Result = *this;
	
	for (FDABoundsShapeCircle& Shape : Result.Circles) {
		Shape.Transform = Shape.Transform * InTransform;
	}

    for (FDABoundsShapeConvexPoly& Shape : Result.ConvexPolys) {
        Shape.Transform = Shape.Transform * InTransform;
    }
	
	return Result;
}


bool FDABoundsShapeCollision::Intersects(const FDABoundsShapeConvexPoly& A, const FDABoundsShapeConvexPoly& B, float Tolerance) {
    if (A.Points.Num() < 3 || B.Points.Num() < 3) {
        return false;
    }
    
    if (!HeightOverlaps(A, B, Tolerance)) {
        return false;
    }

    TArray<FVector2D> PolyA, PolyB;
    TransformPoints(A.Points, A.Transform, PolyA);
    TransformPoints(B.Points, B.Transform, PolyB);
    
    TArray<FVector2D> NormalsToProject;
    {
        TSet<FVector2D> VisitedProjections;
        PopulatePolyProjectionNormals(PolyA, VisitedProjections, NormalsToProject);
        PopulatePolyProjectionNormals(PolyB, VisitedProjections, NormalsToProject);
    }


    // Separating axis algorithm to check for convex poly intersection
    // We project the points of the poly along certain axis and see if the projected points overlap
    // the axis we choose to project are chosen as the normals of the polygon edges
    // This is like projecting shadows of a 3D object on to the 2D ground (or a 2D object on a 1D ground), tested from different light angles
    // If the shadows do not overlap, the objects do not overlap
    
    for (const FVector2D& ProjectionAxis : NormalsToProject) {
        float MinA, MaxA, MinB, MaxB;
        MinA = MinB = TNumericLimits<float>::Max();
        MaxA = MaxB = TNumericLimits<float>::Lowest();

        // Project PolyA
        for (const FVector2D& Point : PolyA) {
            const float Projection = FVector2D::DotProduct(ProjectionAxis, Point);
            MinA = FMath::Min(MinA, Projection);
            MaxA = FMath::Max(MaxA, Projection);
        }
        
        // Project PolyB
        for (const FVector2D& Point : PolyB) {
            const float Projection = FVector2D::DotProduct(ProjectionAxis, Point);
            MinB = FMath::Min(MinB, Projection);
            MaxB = FMath::Max(MaxB, Projection);
        }

        // Check if they don't overlap
        const float OverlapDistance = GetOverlapsDistance(MinA, MaxA, MinB, MaxB);
        if (OverlapDistance < Tolerance) {
            // The points do not overlap.   This means the polygons themselves do not overlap
            return false;
        }
    }
    return true;
}

bool FDABoundsShapeCollision::Intersects(const FDABoundsShapeConvexPoly& A, const FDABoundsShapeCircle& B, float Tolerance) {
    if (A.Points.Num() < 3) {
        return false;
    }
    
    if (!HeightOverlaps(A, B, Tolerance)) {
        return false;
    }


    TArray<FVector2D> PolyA;
    TransformPoints(A.Points, A.Transform, PolyA);
    const FVector2D CenterB = FVector2D(B.Transform.GetLocation());

    TArray<FVector2D> NormalsToProject;
    {
        TSet<FVector2D> VisitedProjections;
        PopulatePolyProjectionNormals(PolyA, VisitedProjections, NormalsToProject);
        PopulateCircleProjectionNormals(CenterB, PolyA, VisitedProjections, NormalsToProject);
    }

    for (const FVector2D& ProjectionAxis : NormalsToProject) {
        float MinA, MaxA, MinB, MaxB;
        MinA = MinB = TNumericLimits<float>::Max();
        MaxA = MaxB = TNumericLimits<float>::Lowest();

        // Project PolyA
        for (const FVector2D& Point : PolyA) {
            const float Projection = FVector2D::DotProduct(ProjectionAxis, Point);
            MinA = FMath::Min(MinA, Projection);
            MaxA = FMath::Max(MaxA, Projection);
        }
        
        // Project CircleB
        const float CenterProjection = FVector2D::DotProduct(ProjectionAxis, CenterB);
        TArray<float> CircleProjections = {
            CenterProjection,
            CenterProjection - B.Radius,
            CenterProjection + B.Radius
        };
        
        for (const float Projection : CircleProjections) {
            MinB = FMath::Min(MinB, Projection);
            MaxB = FMath::Max(MaxB, Projection);
        }

        // Check if they don't overlap
        if (GetOverlapsDistance(MinA, MaxA, MinB, MaxB) < Tolerance) {
            // The points do not overlap.   This means the polygons themselves do not overlap
            return false;
        }
    }
    return true;
}

bool FDABoundsShapeCollision::Intersects(const FDABoundsShapeCircle& A, const FDABoundsShapeCircle& B, float Tolerance) {
    if (!HeightOverlaps(A, B, Tolerance)) {
        return false;
    }

    const FVector2D CenterA(A.Transform.GetLocation());
    const FVector2D CenterB(B.Transform.GetLocation());
    const float DistanceBetweenCenters = (CenterA - CenterB).Size();
    return DistanceBetweenCenters + Tolerance < A.Radius + B.Radius;
}

void FDABoundsShapeCollision::ConvertBoxToConvexPoly(const FBox& InBox, FDABoundsShapeConvexPoly& OutPoly) {
    const FVector2D BoxExtent = FVector2D(InBox.GetExtent());
    const float X = BoxExtent.X;
    const float Y = BoxExtent.Y;
    OutPoly.Points.Add(FVector2D(-X, -Y));
    OutPoly.Points.Add(FVector2D(-X, Y));
    OutPoly.Points.Add(FVector2D(X, Y));
    OutPoly.Points.Add(FVector2D(X, -Y));
    OutPoly.Height = InBox.Max.Z - InBox.Min.Z;
    OutPoly.Transform = FTransform(InBox.GetCenter() - FVector(0, 0, OutPoly.Height * 0.5));
}

void FDABoundsShapeCollision::ConvertBoxToConvexPoly(const FTransform& InTransform, const FVector& InExtent, FDABoundsShapeConvexPoly& OutPoly) {
    const float X = InExtent.X;
    const float Y = InExtent.Y;
    OutPoly.Points.Add(FVector2D(-X, -Y));
    OutPoly.Points.Add(FVector2D(-X, Y));
    OutPoly.Points.Add(FVector2D(X, Y));
    OutPoly.Points.Add(FVector2D(X, -Y));
    OutPoly.Height = InExtent.Z * 2;
    OutPoly.Transform = InTransform;
}

bool FDABoundsShapeCollision::Intersects(const FBox& A, const FDABoundsShapeConvexPoly& B, float Tolerance) {
    if (!BoxHeightOverlaps(A, B, Tolerance)) {
        return false;
    }

    FDABoundsShapeConvexPoly BoxPoly;
    ConvertBoxToConvexPoly(A, BoxPoly);
    return Intersects(BoxPoly, B, Tolerance);
}

bool FDABoundsShapeCollision::Intersects(const FBox& A, const FDABoundsShapeCircle& B, float Tolerance) {
    if (!BoxHeightOverlaps(A, B, Tolerance)) {
        return false;
    }

    const FVector2D Center = FVector2D(B.Transform.GetLocation());
    const FVector2D ClosestPointOnBoxToB = FVector2D(A.GetClosestPointTo(B.Transform.GetLocation()));
    const float DistanceFromBoxEdgeToCenter = (ClosestPointOnBoxToB - Center).Size();
    return DistanceFromBoxEdgeToCenter + Tolerance > B.Radius;
}

bool FDABoundsShapeCollision::Intersects(const FDABoundsShapeList& A, const FDABoundsShapeList& B, float Tolerance) {
    for (const FDABoundsShapeConvexPoly& PolyA : A.ConvexPolys) {
        for (const FDABoundsShapeConvexPoly& PolyB : B.ConvexPolys) {
            if (Intersects(PolyA, PolyB, Tolerance)) {
                return true;
            }
        }

        for (const FDABoundsShapeCircle& CircleB : B.Circles) {
            if (Intersects(PolyA, CircleB, Tolerance)) {
                return true;
            }
        } 
    }

    for (const FDABoundsShapeCircle& CircleA : A.Circles) {
        for (const FDABoundsShapeConvexPoly& PolyB : B.ConvexPolys) {
            if (Intersects(PolyB, CircleA, Tolerance)) {
                return true;
            }
        }

        for (const FDABoundsShapeCircle& CircleB : B.Circles) {
            if (Intersects(CircleA, CircleB, Tolerance)) {
                return true;
            }
        } 
    }
    return false;
}

bool FDABoundsShapeCollision::Intersects(const FDABoundsShapeConvexPoly& InPoly, const FDABoundsShapeLine& InLine, float InTolerance) {
    TArray<FVector2D> PointsPoly, PointsLine;
    TransformPoints(InPoly.Points, InPoly.Transform, PointsPoly);
    TransformPoints({ InLine.LineStart, InLine.LineEnd }, InLine.Transform, PointsLine);

    const FVector2D LineDir = PointsLine[1] - PointsLine[0];
    const float LineLength = LineDir.Length();
    for (int32 Idx = 0; Idx < PointsPoly.Num(); Idx++) {
        FVector2D EdgeDir = PointsPoly[(Idx + 1) % PointsPoly.Num()] - PointsPoly[Idx];
        FVector2D Diff = PointsPoly[Idx] - PointsLine[0];

        const float CrossProductValue = FVector2D::CrossProduct(LineDir, EdgeDir);
        // Check if they are parallel
        if (FMath::IsNearlyEqual(CrossProductValue, 0)) {
            if (InTolerance == 0) {
                const float DotProductValue = FVector2D::DotProduct(Diff, LineDir);
                if (DotProductValue >= 0 && DotProductValue <= FVector2D::DotProduct(LineDir, LineDir)) {
                    return true;
                }
            }
        }
        else {
            const float T1 = FVector2D::CrossProduct(Diff, EdgeDir) / CrossProductValue;
            const float T2 = FVector2D::CrossProduct(Diff, LineDir) / CrossProductValue;

            const float ToleranceT = InTolerance / LineLength;
            if (T1 >= ToleranceT && T1 <= 1 - ToleranceT && T2 >= ToleranceT && T2 <= 1 - ToleranceT) {
                return true; // Intersecting.
            }
        }
    }

    return false;
}

bool FDABoundsShapeCollision::Intersects(const FBox& A, const FDABoundsShapeList& B, float Tolerance) {
    for (const FDABoundsShapeConvexPoly& PolyB : B.ConvexPolys) {
        if (Intersects(A, PolyB, Tolerance)) {
            return true;
        }
    }

    for (const FDABoundsShapeCircle& CircleB : B.Circles) {
        if (Intersects(A, CircleB, Tolerance)) {
            return true;
        }
    }

    return false;
}

void FDABoundsShapeCollision::TransformPoints(const TArray<FVector2D>& InPoints, const FTransform& InTransform,
                                              TArray<FVector2D>& OutTransformedPoints) {
    OutTransformedPoints.Reset();
    OutTransformedPoints.Reserve(InPoints.Num());

    for (const FVector2D& LocalPoint : InPoints) {
        FVector TransformPoint = InTransform.TransformPosition(FVector(LocalPoint, 0));
        OutTransformedPoints.Add(FVector2D(TransformPoint));
    } 
}

void FDABoundsShapeCollision::PopulatePolyProjectionNormals(const TArray<FVector2D>& InTransformedPoints, TSet<FVector2D>& Visited, TArray<FVector2D>& OutProjections) {
    for (int i = 0; i < InTransformedPoints.Num(); i++) {
        const FVector2D& P0 = InTransformedPoints[i];
        const FVector2D& P1 = InTransformedPoints[(i + 1) % InTransformedPoints.Num()];
        const FVector2D Dir = (P1 - P0).GetSafeNormal();
        const FVector2D Normal(-Dir.Y, Dir.X);
        if (!Visited.Contains(Normal)) {
            Visited.Add(Normal);
            Visited.Add(-Normal);
            OutProjections.Add(Normal);
        }
    }
}

void FDABoundsShapeCollision::PopulateCircleProjectionNormals(const FVector2D& InTransformedCenter, const TArray<FVector2D>& InTransformedPolyPoints, TSet<FVector2D>& Visited, TArray<FVector2D>& OutProjections) {
    for (int i = 0; i < InTransformedPolyPoints.Num(); i++) {
        const FVector2D& P0 = InTransformedPolyPoints[i];
        const FVector2D& P1 = InTransformedCenter;
        const FVector2D Dir = (P1 - P0).GetSafeNormal();
        if (!Visited.Contains(Dir)) {
            Visited.Add(Dir);
            Visited.Add(-Dir);
            OutProjections.Add(Dir);
        }
    }
}

