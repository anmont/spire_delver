//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Canvas/DungeonCanvas.h"

#include "Core/Dungeon.h"
#include "Core/Layout/DungeonLayoutData.h"
#include "Core/Utils/DungeonBPFunctionLibrary.h"
#include "Frameworks/Canvas/DungeonCanvasBlueprintGeneratedClass.h"
#include "Frameworks/Canvas/Shaders/CanvasFogOfWarShader.h"
#include "Frameworks/Canvas/Shaders/CanvasShapeBorderGenShader.h"

#include "Async/Async.h"
#include "CanvasItem.h"
#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GlobalRenderResources.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RHIGPUReadback.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "TextureResource.h"
#include "TimerManager.h"
#include "UObject/Package.h"

class UCanvasPanel;
DEFINE_LOG_CATEGORY_STATIC(LogDungeonCanvas, Log, All)

//////////////////////////////// FDungeonCanvasHeightRange ////////////////////////////////
void FDungeonCanvasHeightRange::SetHeightRangeForSingleFloor(const FDungeonFloorSettings& InFloorSettings, int32 FloorIndex) {
	ActiveFloorIndices = { FloorIndex };
	RangeType = FDungeonCanvasHeightRangeType::Floor;
	UpdateFloorRange(InFloorSettings);
}

void FDungeonCanvasHeightRange::SetHeightRangeForMultipleFloors(const FDungeonFloorSettings& InFloorSettings, const TArray<int32>& InFloorIndices) {
	ActiveFloorIndices = InFloorIndices;
	RangeType = FDungeonCanvasHeightRangeType::Floor;
	UpdateFloorRange(InFloorSettings);
}

void FDungeonCanvasHeightRange::SetHeightRangeCustom(float InHeightRangeMin, float InHeightRangeMax) {
	HeightRangeMin = InHeightRangeMin;
	HeightRangeMax = InHeightRangeMax;
	RangeType = FDungeonCanvasHeightRangeType::CustomRange;
}

void FDungeonCanvasHeightRange::SetHeightRangeAcrossAllFloors() {
	HeightRangeMin = -100000000;
	HeightRangeMax = 100000000;
	RangeType = FDungeonCanvasHeightRangeType::EntireDungeon;
}

void FDungeonCanvasHeightRange::UpdateFloorRange(const FDungeonFloorSettings& InFloorSettings) {
	for (int i = 0; i < ActiveFloorIndices.Num(); i++) {
		const int32 FloorIdx = ActiveFloorIndices[i];
		const float Min = FloorIdx * InFloorSettings.FloorHeight + InFloorSettings.GlobalHeightOffset + BaseHeightOffset;
		const float Max = Min + InFloorSettings.FloorCaptureHeight;

		if (i == 0) {
			HeightRangeMin = Min;
			HeightRangeMax = Max;
		}
		else {
			HeightRangeMin = FMath::Min(Min, HeightRangeMin);
			HeightRangeMax = FMath::Max(Max, HeightRangeMax);
		}
	}
}

void UDungeonCanvasCircularFrameActorIconFilter::ApplyFilter(const FDungeonCanvasViewportTransform& View, const TMap<FName,
		FDungeonCanvasOverlayIcon>& OverlayIconMap, const TArray<FName>& ItemTags, TObjectPtr<UObject>& ResourceObject,
		FVector2D& CanvasSize, FVector2D& CanvasLocation, float& ScreenSize, FLinearColor& Tint, float& Rotation)
{
	if (const FDungeonCanvasOverlayIcon* EdgeIconPtr = OverlayIconMap.Find(BoundaryEdgeIconName)) {
		bool bFoundTag = false;
		for (const FName& RequiredTag : TrackedIconTags) {
			if (ItemTags.Contains(RequiredTag)) {
				bFoundTag = true;
				break;
			}
		} 
		if (!bFoundTag) {
			return;
		}
		
		const FVector2D CanvasCenter = CanvasSize * 0.5f;
		const float TargetRadius = CanvasCenter.X * FMath::Clamp(BoundaryRadius, 0, 1);
		const float IconDistanceFromCenter = (CanvasLocation - CanvasCenter).Size();
		const float MaxEdgeIconDisplayDistance = CanvasSize.X * BoundaryEdgeIconMaxDisplayDistance;
		if (IconDistanceFromCenter > TargetRadius && IconDistanceFromCenter < TargetRadius + MaxEdgeIconDisplayDistance) {
			// Clamp to the circle and change the icon
			const FVector2D DirectionFromCenter = (CanvasLocation - CanvasCenter).GetSafeNormal();
			CanvasLocation = CanvasCenter + DirectionFromCenter * TargetRadius;

			ResourceObject = EdgeIconPtr->ResourceObject;
			const float SourceTintAlpha = Tint.A;
			Tint = EdgeIconPtr->Tint;
			Tint.A *= SourceTintAlpha;
			
			Rotation = EdgeIconPtr->RotationOffset;
			const float DirectionAngleRad = FMath::Atan2(DirectionFromCenter.Y, DirectionFromCenter.X);
			Rotation += FMath::RadiansToDegrees(DirectionAngleRad);

			if (EdgeIconPtr->ScreenSizeType == EDungeonCanvasIconCoordinateSystem::Pixels) {
				ScreenSize = EdgeIconPtr->ScreenSize;
			}
			else if (EdgeIconPtr->ScreenSizeType == EDungeonCanvasIconCoordinateSystem::WorldCoordinates) {
				const FVector WorldToCanvasScale = View.GetWorldToCanvas().GetScale3D() * EdgeIconPtr->ScreenSize;
				ScreenSize = FMath::Max(WorldToCanvasScale.X, WorldToCanvasScale.Y);
			}

			if (bEnableScaleBeyondBoundary) {
				const float Ratio = FMath::Clamp((IconDistanceFromCenter - TargetRadius) / MaxEdgeIconDisplayDistance, 0, 1);
				float Scale = 1 - Ratio;
				if (const UCurveFloat* Curve = ScaleCurve.LoadSynchronous()) {
					Scale = Curve->GetFloatValue(Ratio);
				}
				ScreenSize *= Scale;
			}
		}
	}
}

int32 FDungeonCanvasHeightRange::GetFloorIndexAtHeight(const FDungeonFloorSettings& InFloorSettings, float HeightZ) const {
	const float FloorIndexF = (HeightZ - InFloorSettings.GlobalHeightOffset - BaseHeightOffset) / InFloorSettings.FloorHeight;
	const int32 FloorIndex = FMath::FloorToInt(FloorIndexF);
	return FMath::Clamp(FloorIndex, InFloorSettings.FloorLowestIndex, InFloorSettings.FloorHighestIndex);
}

bool FDungeonCanvasHeightRange::InsideActiveFloorHeightRange(float HeightZ) const {
	if (RangeType == FDungeonCanvasHeightRangeType::EntireDungeon) {
		return true;
	}

	return HeightZ >= HeightRangeMin && HeightZ <= HeightRangeMax;
}

//////////////////////////////// DungeonCanvasEffectBase ////////////////////////////////
void UDungeonCanvasEffectBase::Initialize_Implementation() {
}

void UDungeonCanvasEffectBase::Draw_Implementation() {
}

void UDungeonCanvasEffectBase::Tick_Implementation(float DeltaSeconds) {
}

void UDungeonCanvasEffectBase::InitCanvasMaterial_Implementation(UMaterialInstanceDynamic* Material) {
}

void UDungeonCanvasEffectBase::InitFogOfWarMaterial_Implementation(UMaterialInstanceDynamic* Material) {
}

void UDungeonCanvasEffectBase::SetTextures(UCanvasRenderTarget2D* TexLayoutFill, UCanvasRenderTarget2D* TexLayoutBorder, UCanvasRenderTarget2D* TexSDF, UCanvasRenderTarget2D* TexDynamicOcclusion) {
	LayoutFillTexture = TexLayoutFill;
	LayoutBorderTexture = TexLayoutBorder;
	SDFTexture = TexSDF;
	DynamicOcclusionTexture = TexDynamicOcclusion;
}

//////////////////////////////// UDungeonCanvasMaterialLayer ////////////////////////////////
UDungeonCanvasMaterialLayer::UDungeonCanvasMaterialLayer()
	: Effect(nullptr)
{
}

//////////////////////////////// UDungeonCanvas ////////////////////////////////
ADungeonCanvas::ADungeonCanvas()
	: Guid(FGuid::NewGuid())
{
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	RootComponent = SceneRoot;
	
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_DefaultSubObject))
	{
		ClearFlags(RF_Transactional);
	}

	if (HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject)) {
		SetFlags(RF_Public);
	}
	else if (HasAnyFlags(RF_Public)) {
		ClearFlags(RF_Public);
	}
}

void ADungeonCanvas::PostInitializeComponents() {
	Super::PostInitializeComponents();

	if (!Dungeon) {
		Dungeon = Cast<ADungeon>(UGameplayStatics::GetActorOfClass(GetWorld(), ADungeon::StaticClass()));
	}
	
	if (Dungeon) {
		Dungeon->OnDungeonBuildComplete.RemoveDynamic(this, &ADungeonCanvas::OnDungeonBuildComplete);
		Dungeon->OnDungeonBuildComplete.AddDynamic(this, &ADungeonCanvas::OnDungeonBuildComplete);
	}
	else {
		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(0, 20, FColor::Red,
				"ERROR: [DungeonCanvas] Cannot find dungeon actor in the scene. Please assign this in the DungeonCanvas actor. Ignoring initialization");
		}
	}
}

void ADungeonCanvas::OnDungeonBuildComplete(ADungeon* InDungeon, bool bSuccess) {
	if (bSuccess && InDungeon == Dungeon) {
		Initialize();
	}
}


void ADungeonCanvas::SyncFogOfWarItemState() {
	// We want to read the fog of war texture and get the explored / visible state.
	// However, the texture data is in the GPU and downloading it is costly. Instead, we send the
	// actor data over to the gpu shader, and fetch the states and write it out on a small buffer and
	// download that instead
	{
		const EFogOfWarGPUReadbackStage ReadbackStage = FogOfWarGPUReadbackState.GetStage();
		if (ReadbackStage == EFogOfWarGPUReadbackStage::AwaitingRead || ReadbackStage == EFogOfWarGPUReadbackStage::Reading)
		{
			// We already have a request queued up
			return;
		}
	}

	const UCanvasRenderTarget2D* FogOfWarExploredTexture = GetFogOfWarExploredTexture();
	const UCanvasRenderTarget2D* FogOfWarVisibilityTexture = GetFogOfWarVisibilityTexture();
	if (!FogOfWarExploredTexture || !FogOfWarVisibilityTexture) {
		return;
	}
	
	FDungeonCanvasViewportTransform ViewTransform = FullDungeonTransform;
	ViewTransform.FocusOnCanvas(FogOfWarExploredTexture->SizeX, FogOfWarExploredTexture->SizeY);
	
	// Prepare the items to be sent over to the render thread
	TArray<FGuid> UniqueActorIds;
	TArray<FVector2f> ItemLocationsCanvasSpace;
	for (const FDungeonCanvasTrackedActorRegistryItem& Entry : TrackedOverlayActors) {
		if (const AActor* Actor = Entry.TrackedComponent.IsValid() ? Entry.TrackedComponent->GetOwner() : nullptr) {
			UniqueActorIds.Add(Entry.InstanceID);
			const FVector WorldLocation = Actor->GetActorLocation();
			const FVector2D CanvasLocation = ViewTransform.WorldToCanvasLocation(WorldLocation);
			ItemLocationsCanvasSpace.Add(FVector2f(CanvasLocation));
		}
	}

	if (ItemLocationsCanvasSpace.Num() == 0) {
		return;
	}
	

	FogOfWarGPUReadbackState.SetUniqueActorIds(UniqueActorIds);
	const FTextureResource* FoWExploredResource = FogOfWarExploredTexture->GetResource();
	const FTextureResource* FoWVisibilityResource = FogOfWarVisibilityTexture->GetResource();
	
	FogOfWarGPUReadbackState.SetStage(EFogOfWarGPUReadbackStage::AwaitingRead);

	ENQUEUE_RENDER_COMMAND(DAFetchFogOfWarData)(
		[this, ItemLocationsCanvasSpace, FoWExploredResource, FoWVisibilityResource](FRHICommandListImmediate& RHICmdList) {
			if (!FogOfWarGPUReadbackState.ReadbackBuffer.IsValid()) {
				FogOfWarGPUReadbackState.ReadbackBuffer = MakeShareable(new FRHIGPUBufferReadback(TEXT("DungeonCanvas.FogOfWar.ReadbackBuffer")));
			}
			
			const int32 NumBufferElements = ItemLocationsCanvasSpace.Num();
			constexpr int32 ElementSize = sizeof(FVector2f);
			const int32 SizeInBytes = NumBufferElements * ElementSize;
			
			FRDGBuilder GraphBuilder(RHICmdList);
			
			TRefCountPtr<IPooledRenderTarget> FogTexExploredCache;
			TRefCountPtr<IPooledRenderTarget> FogTexVisibilityCache;
			CacheRenderTarget(FoWExploredResource->TextureRHI, TEXT("FoW_FogExploredTex"), FogTexExploredCache);
			CacheRenderTarget(FoWVisibilityResource->TextureRHI, TEXT("FoW_FogVisibilityTex"), FogTexVisibilityCache);
			
			const FRDGTextureRef FogTexExploredRDG = GraphBuilder.RegisterExternalTexture(FogTexExploredCache);
			const FRDGTextureRef FogTexVisibilityRDG = GraphBuilder.RegisterExternalTexture(FogTexVisibilityCache);
			const FRDGTextureSRVRef FogTexExploredSRV = GraphBuilder.CreateSRV(FogTexExploredRDG);
			const FRDGTextureSRVRef FogTexVisibilitySRV = GraphBuilder.CreateSRV(FogTexVisibilityRDG);
			const EPixelFormat PixelFormat = PF_G32R32F;
			
			FRDGBufferSRVRef LocationsSRV;
			{
				FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(ElementSize, NumBufferElements);
				BufferDesc.Usage |= BUF_Static | BUF_ShaderResource;
				const FRDGBufferRef LocationBuffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("DungeonCanvas.FogOfWarQuery.Location"));
				LocationsSRV = GraphBuilder.CreateSRV(LocationBuffer, PixelFormat);

				// Copy data to the buffer
				GraphBuilder.QueueBufferUpload(LocationBuffer, ItemLocationsCanvasSpace.GetData(), ElementSize * NumBufferElements);
			}

			// Allocate the output buffer
			{
				FogOfWarGPUReadbackState.OutputBuffer.SafeRelease();
				FRDGBufferDesc TileBufferDesc = FRDGBufferDesc::CreateStructuredDesc(ElementSize, NumBufferElements);
				TileBufferDesc.Usage |= BUF_UnorderedAccess | BUF_ShaderResource | BUF_SourceCopy;
				AllocatePooledBuffer(TileBufferDesc, FogOfWarGPUReadbackState.OutputBuffer, TEXT("DungeonCanvas.FogOfWarQuery.OutputBuffer"));
			}

			const FRDGBufferRef OutputBuffer = GraphBuilder.RegisterExternalBuffer(FogOfWarGPUReadbackState.OutputBuffer);
			constexpr int32 NUM_THREADS = 64;	// Should match the num threads parameter in the shader
			const FIntVector ThreadGroupsFog = FIntVector(FMath::DivideAndRoundUp(NumBufferElements, NUM_THREADS), 1, 1);
			
			FDACanvasFogOfWarQueryStateShader::FParameters* FogOfWarParams = GraphBuilder.AllocParameters<FDACanvasFogOfWarQueryStateShader::FParameters>();
			FogOfWarParams->TexFogOfWarExplored = FogTexExploredSRV;
			FogOfWarParams->TexFogOfWarVisibility = FogTexVisibilitySRV;
			FogOfWarParams->QueryLocations = LocationsSRV;
			FogOfWarParams->ExploreVisibleStates = GraphBuilder.CreateUAV(OutputBuffer);

			const TShaderMapRef<FDACanvasFogOfWarQueryStateShader> FogOfWarShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("CanvasFogOfWarQueryState"), FogOfWarShader, FogOfWarParams, ThreadGroupsFog);

			AddEnqueueCopyPass(GraphBuilder, FogOfWarGPUReadbackState.ReadbackBuffer.Get(), OutputBuffer, SizeInBytes);
        
			GraphBuilder.Execute();
			
			ReadbackFogOfWarItemState_RenderThread();
		});
}

void ADungeonCanvas::ReadbackFogOfWarItemState_RenderThread() {
	check(IsInRenderingThread());
	
	if (FogOfWarGPUReadbackState.GetStage() == EFogOfWarGPUReadbackStage::AwaitingRead
			&& FogOfWarGPUReadbackState.ReadbackBuffer.IsValid()
			&& FogOfWarGPUReadbackState.ReadbackBuffer->IsReady())
	{
		FogOfWarGPUReadbackState.SetStage(EFogOfWarGPUReadbackStage::Reading);
		TArray<FVector2f> DownloadedItemStates = FogOfWarGPUReadbackState.DownloadReadbackBuffer();
		TArray<FGuid> UniqueActorIds = FogOfWarGPUReadbackState.GetUniqueActorIds();
		FogOfWarGPUReadbackState.SetStage(EFogOfWarGPUReadbackStage::Complete);
		FogOfWarGPUReadbackState.ReadbackBuffer.Reset();
		
		AsyncTask(ENamedThreads::GameThread, [this, DownloadedItemStates, UniqueActorIds]() {
			check(IsInGameThread());

			// Create a lookup by id, for faster access
			TMap<FGuid, FDungeonCanvasTrackedActorRegistryItem*> ExplorersById;
			for (FDungeonCanvasTrackedActorRegistryItem& Entry : TrackedOverlayActors) {
				if (Entry.TrackedComponent.IsValid()) {
					FDungeonCanvasTrackedActorRegistryItem*& LookupValue = ExplorersById.FindOrAdd(Entry.InstanceID);
					LookupValue = &Entry;
				}
			}

			for (int i = 0; i < UniqueActorIds.Num(); i++) {
				const FGuid& ItemId = UniqueActorIds[i];
				if (FDungeonCanvasTrackedActorRegistryItem** SearchResult = ExplorersById.Find(ItemId)) {
					if (FDungeonCanvasTrackedActorRegistryItem* Entry = *SearchResult) {
						const FVector2f& ExploredVisibilityState = DownloadedItemStates[i];
						Entry->FogOfWarExplored = ExploredVisibilityState.X;
						Entry->FogOfWarLineOfSight = ExploredVisibilityState.Y;
					}
				}
			}

		});
	}
}

void ADungeonCanvas::AddTrackedOverlayActor(UDungeonCanvasTrackedObject* TrackedComponent) {
	if (!TrackedComponent) {
		return;
	}
	
	for (FDungeonCanvasTrackedActorRegistryItem& Item : TrackedOverlayActors) {
		if (Item.TrackedComponent == TrackedComponent) {
			// Already added
			return;
		}
	}
	
	// Make sure this object doesn't already exist
	FDungeonCanvasTrackedActorRegistryItem& Item = TrackedOverlayActors.AddDefaulted_GetRef();
	Item.TrackedComponent = TrackedComponent;
	Item.InstanceID = FGuid::NewGuid();
}

void ADungeonCanvas::Initialize(bool bClearCachedResources) {
	// Release existing managed resources
	if (bClearCachedResources) {
		DestroyManagedResources();
	}

	// Cache the dungeon transform
	if (Dungeon && Dungeon->DungeonModel && bCanvasEnabled) {
		DungeonLayout = Dungeon->DungeonModel->DungeonLayout;
		DungeonTransform = Dungeon->GetActorTransform();
		DungeonTransform.RemoveScaling();
		HeightRange.SetBaseHeightOffset(DungeonTransform.GetLocation().Z);
		ReceiveInitialize();
		bInitialized = true;
	}
}
	
void ADungeonCanvas::RequestInitialize() {
	Initialize();
}

void ADungeonCanvas::Draw(const FDungeonCanvasDrawContext& DrawContext) {
	if (bCanvasEnabled) {
		ReceiveDraw(DrawContext);
	}
}

void ADungeonCanvas::RequestDraw(UCanvas* Canvas, int Width, int Height, FDungeonCanvasDrawSettings DrawSettings) {
	if (bCanvasEnabled) {
		FDungeonCanvasViewportTransform FrameViewTransform = ViewportTransform;
		FrameViewTransform.FocusOnCanvas(Width, Height);
		
		FTransform LocalToCanvas = FrameViewTransform.GetLocalToCanvas();
		FQuat Rotation = LocalToCanvas.GetRotation();
		Rotation = Rotation * FQuat(FVector::UpVector, FMath::DegreesToRadians(-DrawSettings.BaseCanvasRotation));
		
		if (DrawSettings.bRotateToView) {
			Rotation = PlayerCanvasRotation.GetInverse().Quaternion() * Rotation;
		}
		
		LocalToCanvas.SetRotation(Rotation);
		FrameViewTransform.SetLocalToCanvas(LocalToCanvas);

		const FDungeonCanvasDrawContext DrawContext{ this, Canvas, FrameViewTransform, DrawSettings, false };
		ReceiveDraw(DrawContext);
	}
}

void ADungeonCanvas::RequestUpdate(float DeltaSeconds) {
	if (bCanvasEnabled) {
		const FDungeonCanvasDrawContext UpdateContext{ this, nullptr, FullDungeonTransform, {}, false };
		ReceiveUpdate(UpdateContext, DeltaSeconds);
	}
}

UCanvasRenderTarget2D* ADungeonCanvas::CreateManagedTexture(int Width, int Height, ETextureRenderTargetFormat Format, FLinearColor ClearColor) {
	UCanvasRenderTarget2D* RenderTarget2D = NewObject<UCanvasRenderTarget2D>(this);
	RenderTarget2D->RenderTargetFormat = Format;
	RenderTarget2D->InitAutoFormat(Width, Height);
	RenderTarget2D->ClearColor = ClearColor;
	RenderTarget2D->UpdateResourceImmediate(true);
	ManagedRenderTargets.Add(RenderTarget2D);
	return RenderTarget2D;
}

UMaterialInstanceDynamic* ADungeonCanvas::CreateMaterialInstance(UMaterialInterface* MaterialTemplate, UObject* Outer) {
	return UMaterialInstanceDynamic::Create(MaterialTemplate, Outer);
}

TArray<FVector2f> ADungeonCanvas::FFogOfWarGPUReadbackState::DownloadReadbackBuffer() {
	check(ReadbackBuffer->IsReady());
	FScopeLock ScopeLock(&CriticalSection);
	const int32 NumElements = UniqueActorIds.Num();
	const int32 SizeInBytes = NumElements * sizeof(FVector2f);
	TArray<FVector2f> DownloadedItemStates;
	DownloadedItemStates.SetNum(NumElements);
	const void* RawData = ReadbackBuffer->Lock(SizeInBytes);
	FMemory::Memcpy(DownloadedItemStates.GetData(), RawData, SizeInBytes);
	ReadbackBuffer->Unlock();
	return DownloadedItemStates;
}

void ADungeonCanvas::DestroyManagedResources() {
	for (UCanvasRenderTarget2D* RenderTarget : ManagedRenderTargets) {
		if (RenderTarget) {
			if (RenderTarget->GetResource()) {
				RenderTarget->ReleaseResource();
			}
			RenderTarget->MarkAsGarbage();
		}
	}
	ManagedRenderTargets.Reset();
}

void ADungeonCanvas::Tick(float DeltaTime) {
	if (!bInitialized) {
		return;
	}
	
	TickPlayer();
	
	if (bCanvasEnabled) {
		ReceiveTick(DeltaTime);
		RequestUpdate(DeltaTime);
	}
	
	ENQUEUE_RENDER_COMMAND(DungeonCanvasFogOfWarReadback)(
		[this](FRHICommandListImmediate& RHICmdList) {
			ReadbackFogOfWarItemState_RenderThread();
		});
	
	// Cleanup
	ManagedRenderTargets.RemoveAll([](const UCanvasRenderTarget2D* RenderTarget) { return RenderTarget == nullptr; });
	TrackedOverlayActors.RemoveAll([](const FDungeonCanvasTrackedActorRegistryItem& Item) { return !Item.TrackedComponent.IsValid(); });
	FogOfWarExplorers.RemoveAll([](const FFogOfWarItemEntry& Item) { return !Item.ActorPtr.IsValid(); });
}

void ADungeonCanvas::BeginPlay() {
	Super::BeginPlay();

	{
		FogOfWarItemStateUpdateFPS = FMath::Max(FogOfWarItemStateUpdateFPS, 1);
		const float FoWActorUpdateInterval = 1.0f / FogOfWarItemStateUpdateFPS;
		GetWorld()->GetTimerManager().SetTimer(FogOfWarTimerHandle, this,  &ADungeonCanvas::SyncFogOfWarItemState, FoWActorUpdateInterval, true);
	}

	
}

void ADungeonCanvas::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearTimer(FogOfWarTimerHandle);
}

//////////////////////////////// UDungeonCanvas ////////////////////////////////

class FDungeonCanvasRenderUtils {
public:
	typedef TFunction<void(UCanvas*, const FVector2D&)> TCanvasRenderCallback; 
	static void RenderCanvasTexture(UWorld* InWorld, UTextureRenderTarget2D* OutTexture, TCanvasRenderCallback Callback, bool bClear = false, const FLinearColor& InClearColor = FLinearColor::Black) {
		if (bClear) {
			UKismetRenderingLibrary::ClearRenderTarget2D(InWorld, OutTexture, FLinearColor::Black);
		}
		
		FVector2D CanvasSize;
		UCanvas* Canvas = nullptr;
		FDrawToRenderTargetContext RenderContext;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(InWorld, OutTexture, Canvas, CanvasSize, RenderContext);

		Callback(Canvas, CanvasSize);
		
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(InWorld, RenderContext);
		OutTexture->Modify();
	}
	
	static void DrawShapesFill(UWorld* InWorld, const FDungeonLayoutDataChunkInfo& LayoutShapes, const FDungeonCanvasViewportTransform& ViewportTransform,
			UTextureRenderTarget2D* OutTexture, bool bClearRTT = true, const FLinearColor& InClearColor = FLinearColor::Black, const FTexture* ShapeTexture = GWhiteTexture, ESimpleElementBlendMode BlendMode = SE_BLEND_Additive,
			TFunction<FLinearColor(float)> FnGetPolyColor = [](float Height){ return FLinearColor::White; })
	{
		RenderCanvasTexture(InWorld, OutTexture, [&LayoutShapes, &ViewportTransform, ShapeTexture, BlendMode, FnGetPolyColor](UCanvas* Canvas, const FVector2D& CanvasSize) {
			// Calculate the bounds and create the texture transformer (to move world space coords to texture space coords)

			const FTransform WorldToCanvasTransform = ViewportTransform.GetWorldToCanvas();
			auto WorldToCanvas = [&WorldToCanvasTransform](const FVector& InVector)-> FVector2D {
				return FVector2D(WorldToCanvasTransform.TransformPosition(InVector));
			};
			
			//  Draw the layout shapes in the fill texture mask
			for (const FDABoundsShapeConvexPoly& ConvexPoly : LayoutShapes.ConvexPolys) {
				FLinearColor Color;
				{
					FTransform ColorTransform = FTransform(FVector(0, 0, ConvexPoly.Height)) * ConvexPoly.Transform;
					Color = FnGetPolyColor(ColorTransform.GetLocation().Z);
				}
				for (int i = 2; i < ConvexPoly.Points.Num(); i++) {
					const int i0 = 0;
					const int i1 = i - 1;
					const int i2 = i;
					FVector2D P0 = WorldToCanvas(ConvexPoly.Transform.TransformPosition(FVector(ConvexPoly.Points[i0], 0)));
					FVector2D P1 = WorldToCanvas(ConvexPoly.Transform.TransformPosition(FVector(ConvexPoly.Points[i1], 0)));
					FVector2D P2 = WorldToCanvas(ConvexPoly.Transform.TransformPosition(FVector(ConvexPoly.Points[i2], 0)));

					FCanvasTriangleItem Triangle(P0, P1, P2, ShapeTexture);
					Triangle.SetColor(Color);
					Canvas->DrawItem(Triangle);
				}
			}
			
			for (const FDABoundsShapeCircle& Circle : LayoutShapes.Circles) {
				FVector2D P0 = WorldToCanvas(Circle.Transform.GetLocation());
				const float Radius = Circle.Radius; // * Circle.Transform.GetScale3D().X;
				FLinearColor Color;
				{
					FTransform ColorTransform = FTransform(FVector(0, 0, Circle.Height)) * Circle.Transform;
					Color = FnGetPolyColor(ColorTransform.GetLocation().Z);
				}
				constexpr int32 NumSegments = 32;
				for (int i = 1; i <= NumSegments; i++) {
					float AngleRad0 = (PI * 2) / NumSegments * i;
					float AngleRad1 = (PI * 2) / NumSegments * (i - 1);
					FVector2D P1 = WorldToCanvas(Circle.Transform.TransformPosition(Radius * FVector(FMath::Cos(AngleRad0), FMath::Sin(AngleRad0), 0)));
					FVector2D P2 = WorldToCanvas(Circle.Transform.TransformPosition(Radius * FVector(FMath::Cos(AngleRad1), FMath::Sin(AngleRad1), 0)));
					
					FCanvasTriangleItem Triangle(P0, P1, P2, ShapeTexture);
					Triangle.SetColor(Color);
					Canvas->DrawItem(Triangle);
				}
			}

			for (const FDungeonCanvasRoomShapeTexture& TextureShape : LayoutShapes.CanvasShapeTextures) {
				FTextureResource* TexResource = TextureShape.TextureMask
					? TextureShape.TextureMask->GetResource()
					: nullptr;

				if (!TexResource) {
					continue;
				}

				TArray<FVector2D> WorldLocations;
				WorldLocations.SetNum(4);
				for (int i = 0; i < 4; i++) {
					WorldLocations[i] = WorldToCanvas(TextureShape.Transform.TransformPosition(FDungeonCanvasRoomShapeTexture::LocalQuadPoints[i]));
				}
				
				FLinearColor Color = FnGetPolyColor(TextureShape.Transform.GetLocation().Z);
				for (int i = 2; i < 4; i++) {
					const FVector2D& P0 = WorldLocations[0];
					const FVector2D& P1 = WorldLocations[i - 1];
					const FVector2D& P2 = WorldLocations[i];

					const FVector2D& T0 = FDungeonCanvasRoomShapeTexture::QuadUV[0];
					const FVector2D& T1 = FDungeonCanvasRoomShapeTexture::QuadUV[i - 1];
					const FVector2D& T2 = FDungeonCanvasRoomShapeTexture::QuadUV[i];

					FCanvasTriangleItem Triangle(P0, P1, P2, T0, T1, T2, TexResource);
					Triangle.SetColor(Color);
					Triangle.BlendMode = BlendMode;
					Canvas->DrawItem(Triangle);
				}
			}
			
		}, bClearRTT, InClearColor);
	}
};

FTransform ADungeonCanvas::DrawDungeonLayout(UTextureRenderTarget2D* OutLayoutFill, UTextureRenderTarget2D* OutLayoutBorder)
{
	const FDungeonLayoutData& UnfilteredDungeonLayout = DungeonLayout;
	const FDungeonLayoutData FilteredDungeonLayout = FDungeonLayoutUtils::FilterByHeightRange(UnfilteredDungeonLayout, HeightRange.GetMinHeight(), HeightRange.GetMaxHeight()); 
	if (!OutLayoutFill || !OutLayoutBorder) {
		return {};
	}

	if (OutLayoutFill->GetSurfaceWidth() != OutLayoutBorder->GetSurfaceWidth()
			|| OutLayoutFill->GetSurfaceHeight() != OutLayoutBorder->GetSurfaceHeight())
	{
		return {};
	}

	UWorld* World = GetWorld();
	const FVector2D CanvasSize(OutLayoutFill->GetSurfaceWidth(), OutLayoutFill->GetSurfaceHeight());
	
	// Accumulate all the shapes from the chunks and render them in a single batch
	FDungeonLayoutDataChunkInfo AllShapes;
	for (const FDungeonLayoutDataChunkInfo& ChunkShapeList : FilteredDungeonLayout.ChunkShapes) {
		AllShapes.Append(ChunkShapeList, { FDABoundsShapeConstants::TagDoNotRenderOnCanvas });
	}

	FBox DungeonLayoutBounds = UDungeonBPFunctionLibrary::GenerateDungeonLayoutBounds(UnfilteredDungeonLayout, LayoutDrawMarginPercent);
	FullDungeonTransform = {};
	FullDungeonTransform.FocusOnCanvas(CanvasSize.X, CanvasSize.Y);
	FullDungeonTransform.SetLocalToWorld(FTransform(FRotator::ZeroRotator, DungeonLayoutBounds.GetCenter(), DungeonLayoutBounds.GetSize()));

	// Draw the height in the layout texture, normalized from (0..1]    O here would imply empty, so the lowest floor will be greater than 0
	float MinHeight{};
	float MaxHeight{};
	{
		bool bRangeInitialized{};
		auto UpdateRange = [&](float InHeight) {
			if (!bRangeInitialized) {
				MinHeight = MaxHeight = InHeight;
				bRangeInitialized = true;
			}
			else {
				MinHeight = FMath::Min(MinHeight, InHeight);
				MaxHeight = FMath::Max(MaxHeight, InHeight);
			}
		};
		
		for (const FDungeonLayoutDataChunkInfo& ChunkShape : DungeonLayout.ChunkShapes) {
			for (const FDABoundsShapeConvexPoly& ConvexPoly : ChunkShape.ConvexPolys) {
				FTransform Transform = FTransform(FVector(0, 0, ConvexPoly.Height)) * ConvexPoly.Transform;
				UpdateRange(Transform.GetLocation().Z);
			}
			for (const FDABoundsShapeCircle& Circle : ChunkShape.Circles) {
				FTransform Transform = FTransform(FVector(0, 0, Circle.Height)) * Circle.Transform;
				UpdateRange(Transform.GetLocation().Z);
			}
			for (const FDungeonCanvasRoomShapeTexture& CanvasShapeTexture : ChunkShape.CanvasShapeTextures) {
				UpdateRange(CanvasShapeTexture.Transform.GetLocation().Z);
			}
		}
	}
	
	auto FnGetPolyColor = [MinHeight, MaxHeight](float Height) {
		Height = FMath::Clamp(Height, MinHeight, MaxHeight);
		float Alpha{};
		if (!FMath::IsNearlyEqual(MinHeight, MaxHeight)) {
			Alpha = (Height - MinHeight) / (MaxHeight - MinHeight);
			Alpha = Alpha * 0.9f + 0.1f;
		}
		else {
			Alpha = 1;
		}
		return FLinearColor(Alpha, Alpha, Alpha, 1);
	};
	
	FDungeonCanvasRenderUtils::DrawShapesFill(World, AllShapes, FullDungeonTransform, OutLayoutFill, true, FLinearColor::Black, GWhiteTexture, SE_BLEND_Additive, FnGetPolyColor);
	UKismetRenderingLibrary::ClearRenderTarget2D(World, OutLayoutBorder, FLinearColor::Black);
	
	// Make a temporary fill texture
	UCanvasRenderTarget2D* TempFillTex = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(World, UCanvasRenderTarget2D::StaticClass(),
			OutLayoutFill->GetSurfaceWidth(), OutLayoutFill->GetSurfaceHeight());
	
	FTextureResource* TempFillTexResource = TempFillTex->GetResource();
	FTextureResource* LayoutBorderResource = OutLayoutBorder->GetResource();
	
	// Draw the border for each chunk
	for (const FDungeonLayoutDataChunkInfo& ChunkShapeList : FilteredDungeonLayout.ChunkShapes) {
		FDungeonLayoutDataChunkInfo FilteredShapes;
		FilteredShapes.Append(ChunkShapeList, { FDABoundsShapeConstants::TagDoNotRenderOnCanvas });
		// Fill the chunk shapes, then find the combined outline for that shape
		FDungeonCanvasRenderUtils::DrawShapesFill(World, FilteredShapes, FullDungeonTransform, TempFillTex, true);

		ENQUEUE_RENDER_COMMAND(WriteChunkOutlines)(
			[FilteredShapes, FullDungeonTransform = FullDungeonTransform, TempFillTexResource, LayoutBorderResource, CanvasSize](FRHICommandListImmediate& RHICmdList) {
				FRDGBuilder GraphBuilder(RHICmdList);
				
				auto WorldToCanvas = [&FullDungeonTransform](const FVector& InVector)-> FVector2D {
					const FTransform WorldToCanvasTransform = FullDungeonTransform.GetWorldToCanvas();
					return FVector2D(WorldToCanvasTransform.TransformPosition(InVector));
				};
				
				// Find the bounds of the chunk that we just wrote and run it through the edge detection shader
				FIntVector2 CanvasChunkMin, CanvasChunkSize;
				{
					FDungeonLayoutUtils::FCalcBoundsSettings CalcBoundsSettings;
					CalcBoundsSettings.bIncludeTextureOverlays = true;
					const FBox ChunkBounds = FDungeonLayoutUtils::CalculateBounds(FilteredShapes, CalcBoundsSettings);
		
					const FVector2D CanvasChunkMinF = WorldToCanvas(ChunkBounds.Min);
					const FVector2D CanvasChunkMaxF = WorldToCanvas(ChunkBounds.Max);

					CanvasChunkMin = FIntVector2(
						FMath::FloorToInt(CanvasChunkMinF.X),
						FMath::FloorToInt(CanvasChunkMinF.Y));

					const FIntVector2 CanvasChunkMax = FIntVector2(
						FMath::CeilToInt(CanvasChunkMaxF.X),
						FMath::CeilToInt(CanvasChunkMaxF.Y));
		
					CanvasChunkSize = CanvasChunkMax - CanvasChunkMin;
				}

				TRefCountPtr<IPooledRenderTarget> TempFillTexCache, LayoutBorderCache;
				CacheRenderTarget(TempFillTexResource->TextureRHI, TEXT("BorderGen_FillTex"), TempFillTexCache);
				CacheRenderTarget(LayoutBorderResource->TextureRHI, TEXT("BorderGen_BorderTex"), LayoutBorderCache);
				
				const FRDGTextureRef FillTexRDG = GraphBuilder.RegisterExternalTexture(TempFillTexCache);
				const FRDGTextureRef BorderTexRDG = GraphBuilder.RegisterExternalTexture(LayoutBorderCache);

				const FRDGTextureSRVRef FillTexSRV = GraphBuilder.CreateSRV(FillTexRDG);
				const FRDGTextureUAVRef BorderTexUAV = GraphBuilder.CreateUAV(BorderTexRDG);

				FDACanvasShapeBorderGenShader::FParameters* BorderGenParams = GraphBuilder.AllocParameters<FDACanvasShapeBorderGenShader::FParameters>();
				BorderGenParams->TextureWidth = CanvasSize.X;
				BorderGenParams->TextureHeight = CanvasSize.Y;

				BorderGenParams->OffsetX = CanvasChunkMin.X;
				BorderGenParams->OffsetY = CanvasChunkMin.Y;
				
				BorderGenParams->FillTexture = FillTexSRV;
				BorderGenParams->BorderTexture = BorderTexUAV;

				const int32 ThreadGroupsPerSideX = FMath::DivideAndRoundUp(CanvasChunkSize.X, 16);
				const int32 ThreadGroupsPerSideY = FMath::DivideAndRoundUp(CanvasChunkSize.Y, 16);
				const FIntVector ThreadGroups(ThreadGroupsPerSideX, ThreadGroupsPerSideY, 1);
				
				const TShaderMapRef<FDACanvasShapeBorderGenShader> BorderGenShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BorderGen"), BorderGenShader, BorderGenParams, ThreadGroups);

				GraphBuilder.Execute();
			});
	}

	TempFillTex->ReleaseResource();
	TempFillTex = nullptr;

	/*
	 * For the doors:
	 * Draw black boxes on the outline texture (to remove the outlines where the doors occur)
	 * Draw white boxes on the fill texture, so disconnected rooms are connected by thick doors
	 * Finally, run the outline shader again on all the door bounds to connect the outlines of the doors, if the rooms were disconnected
	 */
	{
		FDungeonLayoutDataChunkInfo DoorShapes;
		for (const FDungeonLayoutDataDoorItem& DoorInfo : FilteredDungeonLayout.Doors) {
			FVector2D DoorBoxSize(DoorInfo.Width, DoorInfo.DoorOcclusionThickness);
			const TArray<FVector2D> LocalDoorPoints = {
				DoorBoxSize * FVector2D(-0.5, -0.5),
				DoorBoxSize * FVector2D( 0.5, -0.5),
				DoorBoxSize * FVector2D( 0.5,  0.5),
				DoorBoxSize * FVector2D(-0.5,  0.5),
			};
		
			FDABoundsShapeConvexPoly& DoorPoly = DoorShapes.ConvexPolys.AddDefaulted_GetRef();
			DoorPoly.Points = LocalDoorPoints;
			DoorPoly.Transform = FTransform(FRotator(0, 90, 0)) * DoorInfo.WorldTransform;
		}
		// Clear the outlines by drawing a black box around the doors
		FDungeonCanvasRenderUtils::DrawShapesFill(World, DoorShapes, FullDungeonTransform, OutLayoutBorder, false, FLinearColor::Black, GBlackTexture);
	}

	// Draw the stair openings
	{
		FDungeonLayoutDataChunkInfo StairShapes;
		for (const FDungeonLayoutDataStairItem& StairInfo : FilteredDungeonLayout.Stairs) {
			constexpr float StairSizeMultiplier = 0.9f;
			FVector2D StairBoxSize(StairInfo.Width * StairSizeMultiplier, StairInfo.Width * StairSizeMultiplier);
			const TArray<FVector2D> LocalDoorPoints = {
				StairBoxSize * FVector2D(-0.5, -0.5),
				StairBoxSize * FVector2D( 0.5, -0.5),
				StairBoxSize * FVector2D( 0.5,  0.5),
				StairBoxSize * FVector2D(-0.5,  0.5),
			};
		
			FDABoundsShapeConvexPoly& StairPoly = StairShapes.ConvexPolys.AddDefaulted_GetRef();
			StairPoly.Points = LocalDoorPoints;
			StairPoly.Transform = FTransform(FRotator(0, 90, 0), FVector(-StairInfo.Width * 0.5, 0, 0)) * StairInfo.WorldTransform;
		}
		// Clear the outlines by drawing a black box around the doors
		FDungeonCanvasRenderUtils::DrawShapesFill(World, StairShapes, FullDungeonTransform, OutLayoutBorder, false, FLinearColor::Black, GBlackTexture);
	}
	
	return FullDungeonTransform.GetLocalToWorld();
}

/*
void UDungeonCanvas::DrawDungeonLayoutCustomLayer(const FDungeonLayoutData& DungeonLayout, const FBox2D& DrawBounds,
		UTextureRenderTarget2D* OutCustomLayerTexture, FDungeonCanvasDrawLayoutSettings DrawSettings, bool bShouldClearColor, FLinearColor ClearColor)
{
	if (!OutCustomLayerTexture) {
		return;
	}
	
	UWorld* World = GetWorld();
	const FVector2D CanvasSize(OutCustomLayerTexture->GetSurfaceWidth(), OutCustomLayerTexture->GetSurfaceHeight());
	
	// Accumulate all the shapes from the chunks and render them in a single batch
	FDungeonLayoutDataChunkInfo AllShapes;
	for (const FDungeonLayoutDataChunkInfo& ChunkShapeList : DungeonLayout.ChunkShapes) {
		for (const FDungeonCanvasOverlayTexture& TextureOverlay : ChunkShapeList.CanvasTextureOverlays) {
			if (TextureOverlay.Tags.Contains(FDABoundsShapeConstants::TagDoNotRenderOnCanvas)) {
				continue;
			}
			AllShapes.CanvasTextureOverlays.Add(TextureOverlay);
		}
	}
	
	FDungeonCanvasViewportTransform& CanvasViewport = ViewportTransform;
	CanvasViewport.SetWorldBounds(DrawBounds);
	CanvasViewport.UpdateCanvasSize(CanvasSize);

	FDungeonCanvasRenderUtils::DrawShapesFill(World, AllShapes, CanvasViewport, OutCustomLayerTexture, bShouldClearColor, ClearColor, GWhiteTexture, SE_BLEND_Translucent);
}
*/

void ADungeonCanvas::SetCameraLocation(FVector WorldLocation) {
	FTransform LocalToWorld = ViewportTransform.GetLocalToWorld();
	LocalToWorld.SetLocation(WorldLocation);
	ViewportTransform.SetLocalToWorld(LocalToWorld);
}

void ADungeonCanvas::SetPlayerCanvasRotation(FRotator CanvasRotation) {
	PlayerCanvasRotation = CanvasRotation;
}

void ADungeonCanvas::SetCameraSize(FVector WorldSize) {
	FTransform LocalToWorld = ViewportTransform.GetLocalToWorld();
	LocalToWorld.SetScale3D(WorldSize);
	ViewportTransform.SetLocalToWorld(LocalToWorld);	
}

void ADungeonCanvas::SetCameraWorldTransform(FTransform WorldTransform) {
	ViewportTransform.SetLocalToWorld(WorldTransform); 
}

void ADungeonCanvas::SetHeightRangeForSingleFloor(int32 FloorIndex) {
	HeightRange.SetHeightRangeForSingleFloor(DungeonLayout.FloorSettings, FloorIndex);
}

int32 ADungeonCanvas::GetFloorIndexAtHeight(float HeightZ) const {
	return HeightRange.GetFloorIndexAtHeight(DungeonLayout.FloorSettings, HeightZ);
}

bool ADungeonCanvas::InsideActiveFloorHeightRange(float HeightZ) const {
	return HeightRange.InsideActiveFloorHeightRange(HeightZ);
}

void ADungeonCanvas::SetHeightRangeForMultipleFloors(const TArray<int32>& InFloorIndices) {
	HeightRange.SetHeightRangeForMultipleFloors(DungeonLayout.FloorSettings, InFloorIndices);
}

void ADungeonCanvas::SetHeightRangeAcrossAllFloors() {
	HeightRange.SetHeightRangeAcrossAllFloors();
}

void ADungeonCanvas::SetHeightRangeCustom(float InHeightRangeMin, float InHeightRangeMax) {
	HeightRange.SetHeightRangeCustom(InHeightRangeMin, InHeightRangeMax);
}

void ADungeonCanvas::AddFogOfWarExplorer(AActor* Actor, FDungeonCanvasItemFogOfWarSettings Settings) {
	for (FFogOfWarItemEntry& ExistingEntry : FogOfWarExplorers) {
		if (ExistingEntry.ActorPtr == Actor) {
			ExistingEntry.Settings = Settings;
			return;
		}
	}
	
	FogOfWarExplorers.Add({Actor, Settings});
}

UCanvasRenderTarget2D* ADungeonCanvas::GetFogOfWarExploredTexture_Implementation() {
	return nullptr;
}
 
UCanvasRenderTarget2D* ADungeonCanvas::GetFogOfWarVisibilityTexture_Implementation() {
	return nullptr;
}

void ADungeonCanvas::SetupPlayer_Implementation(APawn* NewPlayerPawn, FDungeonCanvasItemFogOfWarSettings FogOfWarSettings) {
	
}

void ADungeonCanvas::TickPlayer_Implementation() {
	
}

UMaterialInterface* ADungeonCanvas::GetCanvasBaseMaterial() const {
	const UDungeonCanvasBlueprintGeneratedClass* CanvasGeneratedClass = Cast<UDungeonCanvasBlueprintGeneratedClass>(GetClass());
	return CanvasGeneratedClass ? CanvasGeneratedClass->MaterialInstance : nullptr;

	//return CompiledMaterialInstance;
}

