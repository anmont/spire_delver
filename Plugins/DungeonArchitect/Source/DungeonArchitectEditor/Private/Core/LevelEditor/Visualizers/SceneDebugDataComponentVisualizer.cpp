//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/LevelEditor/Visualizers/SceneDebugDataComponentVisualizer.h"

#include "Core/Utils/Debug/SceneDebugDataComponent.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "SceneManagement.h"

void FDASceneDebugDataComponentVisualizer::DrawVisualizationHUD(const UActorComponent* InComponent, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) {
	const UDASceneDebugDataComponent* Component = Cast<UDASceneDebugDataComponent>(InComponent);
	if (!Component) return;

	const FVector& Scale = Component->RenderScale;
		
	const AActor* Owner = Component->GetOwner();
	if (!Owner) return;
		
	const UFont* Font = GEngine->GetMediumFont();
	const FDASceneDebugData& DebugData = Component->Data;
	for (const FDASceneDebugDataTextEntry& TextEntry : DebugData.TextEntries) {
		const FVector WorldLocation = TextEntry.Location * Scale;
		const FPlane Proj = View->Project(WorldLocation);
		if (Proj.W > 0) {
			const int32 HalfX = 0.5f * Viewport->GetSizeXY().X;
			const int32 HalfY = 0.5f * Viewport->GetSizeXY().Y;
        
			const int32 XPos = HalfX + ( HalfX * Proj.X );
			const int32 YPos = HalfY + ( HalfY * (Proj.Y * -1.f) );

			const FVector2D BaseLocation(XPos, YPos);

			FCanvasTextItem TextItem(BaseLocation, TextEntry.Text, Font, TextEntry.Color);
			TextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem( TextItem );
		}
	} 
}

void FDASceneDebugDataComponentVisualizer::DrawVisualization(const UActorComponent* InComponent, const FSceneView* View, FPrimitiveDrawInterface* PDI) {
	FComponentVisualizer::DrawVisualization(InComponent, View, PDI);
	const UDASceneDebugDataComponent* Component = Cast<UDASceneDebugDataComponent>(InComponent);
	if (!Component) return;
		
	const AActor* Owner = Component->GetOwner();
	if (!Owner) return;

	const FVector& Scale = Component->RenderScale;
	const FDASceneDebugData& DebugData = Component->Data;
	for (const FDASceneDebugDataSphereEntry& SphereEntry : DebugData.SphereEntries) {
		DrawCircle(PDI, SphereEntry.Center * Scale, FVector::XAxisVector, FVector::YAxisVector,
				SphereEntry.Color.ToFColor(false), SphereEntry.Radius * Scale.X, 32, SDPG_Foreground);
	}
	
	for (const FDASceneDebugDataBoxEntry& BoxEntry : DebugData.BoxEntries) {
		DrawOrientedWireBox(PDI, BoxEntry.Transform.GetLocation(),
			BoxEntry.Transform.GetScaledAxis(EAxis::X),
			BoxEntry.Transform.GetScaledAxis(EAxis::Y),
			BoxEntry.Transform.GetScaledAxis(EAxis::Z),
			BoxEntry.Extent, BoxEntry.Color, SDPG_Foreground);
	}
}

