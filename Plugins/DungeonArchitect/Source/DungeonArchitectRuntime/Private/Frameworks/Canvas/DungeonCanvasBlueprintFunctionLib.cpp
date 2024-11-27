//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Canvas/DungeonCanvasBlueprintFunctionLib.h"

#include "Core/Layout/DungeonDoorInterface.h"
#include "Frameworks/Canvas/DungeonCanvas.h"
#include "Frameworks/Canvas/DungeonCanvasStructs.h"
#include "Frameworks/Canvas/Shaders/BlitShader.h"
#include "Frameworks/Canvas/Shaders/CanvasFogOfWarShader.h"
#include "Frameworks/Canvas/Shaders/CanvasSDFEffectShaders.h"
#include "Frameworks/Canvas/Shaders/ConvoluteShader.h"
#include "Frameworks/Canvas/Shaders/JFAShader.h"

#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GlobalRenderResources.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "TextureResource.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogDungeonCanvasLib, Log, All);

namespace DungeonCanvasBlueprintFunctionLib {
	void DrawCanvasIcon(UCanvas* Canvas, UObject* ResourceObject, const FVector2D& InLocation, float ScreenSize, float AspectRatio,
							   FLinearColor InColor, float Rotation = 0.0f, EBlendMode BlendMode = BLEND_Masked) {
		if (!Canvas || !ResourceObject) return;
		if (FMath::IsNearlyZero(AspectRatio)) {
			AspectRatio = 1.0f;
		}
		
		float Width{};
		float Height{};
		
		if (AspectRatio >= 1.0f) {
			Width = ScreenSize;
			Height = ScreenSize / AspectRatio;
		}
		else {
			Width = ScreenSize * AspectRatio;
			Height = ScreenSize;
		}
		
		const FVector2D Size = FVector2D(Width, Height);
		const FVector2D CanvasLocation = InLocation - Size * 0.5f;

		if (UTexture* Texture = Cast<UTexture>(ResourceObject)) {
			Canvas->K2_DrawTexture(Texture, CanvasLocation, Size,
										   FVector2D::ZeroVector, FVector2D::UnitVector, InColor, BlendMode, Rotation);
		}
		else if (UMaterialInterface* Material = Cast<UMaterialInterface>(ResourceObject)) {
			Canvas->K2_DrawMaterial(Material, CanvasLocation, Size, FVector2D::ZeroVector, FVector2D::UnitVector, Rotation);
		}
	}

	template<typename TShader>
	void TBlurTexture(UCanvasRenderTarget2D* SourceTexture, UCanvasRenderTarget2D* DestinationTexture) {
		if (!SourceTexture || !DestinationTexture
				|| SourceTexture->SizeX != DestinationTexture->SizeX
				|| SourceTexture->SizeY != DestinationTexture->SizeY) {
			return;
				}
				
		const FIntPoint TextureSize(SourceTexture->SizeX, SourceTexture->SizeY);
	
		const FIntVector ThreadGroupsFogBlur = FIntVector(
				FMath::DivideAndRoundUp(TextureSize.X, 16),
				FMath::DivideAndRoundUp(TextureSize.Y, 16),
				1);

	
		const FTextureResource* SourceTexResource = SourceTexture->GetResource();
		const FTextureResource* DestTexResource = DestinationTexture->GetResource();
	
		ENQUEUE_RENDER_COMMAND(DABlurTexture)(
			[=](FRHICommandListImmediate& RHICmdList) {
				FRDGBuilder GraphBuilder(RHICmdList);
			
				TRefCountPtr<IPooledRenderTarget> SrcTexCache, DstTexCache;
				CacheRenderTarget(SourceTexResource->TextureRHI, TEXT("Blur_SrcTex"), SrcTexCache);
				CacheRenderTarget(DestTexResource->TextureRHI, TEXT("Blur_DstTex"), DstTexCache);
			
				const FRDGTextureRef SrcTexRDG = GraphBuilder.RegisterExternalTexture(SrcTexCache);
				const FRDGTextureRef DstTexRDG = GraphBuilder.RegisterExternalTexture(DstTexCache);

				const FRDGTextureSRVRef SrcTexSRV = GraphBuilder.CreateSRV(SrcTexRDG);
				const FRDGTextureUAVRef DstTexUAV = GraphBuilder.CreateUAV(DstTexRDG);

				typename TShader::FParameters* FogBlurParams = GraphBuilder.AllocParameters<typename TShader::FParameters>();
				FogBlurParams->InTexture = SrcTexSRV;
				FogBlurParams->OutTexture = DstTexUAV;
				FogBlurParams->TextureWidth = TextureSize.X;
				FogBlurParams->TextureHeight = TextureSize.Y;
					
				const TShaderMapRef<TShader> FogBlurShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("CanvasBlur"), FogBlurShader, FogBlurParams, ThreadGroupsFogBlur);

				GraphBuilder.Execute();
			});
	}
}

void UDungeonCanvasBlueprintFunctionLib::GenerateSDF(UTexture* FillTexture, UTexture* BorderTexture, UTexture* DynamicOcclusionTexture, UCanvasRenderTarget2D* SDFTexture) {
	if (!FillTexture || !SDFTexture) {
		return;
	}
	if (FillTexture->GetSurfaceWidth() != SDFTexture->GetSurfaceWidth()
			|| FillTexture->GetSurfaceHeight() != SDFTexture->GetSurfaceHeight()) {
		return;
	}
  
	FTexture* MaskResource = FillTexture->GetResource();
	FTexture* BorderResource = BorderTexture->GetResource();
	FTexture* SDFResource = SDFTexture->GetResource();
	FTexture* DynamicOcclusionResource = DynamicOcclusionTexture ? DynamicOcclusionTexture->GetResource() : GBlackTexture;
	if (!MaskResource || !SDFResource || !DynamicOcclusionResource) {
		return;
	}
	
	const int32 SurfaceWidth = FillTexture->GetSurfaceWidth();
	const int32 SurfaceHeight = FillTexture->GetSurfaceHeight();

	SDFTexture->Modify();
	ENQUEUE_RENDER_COMMAND(DAGenerateSDF)(
		[MaskResource, BorderResource, SDFResource, DynamicOcclusionResource, SurfaceWidth, SurfaceHeight]
			(FRHICommandListImmediate& RHICmdList){
		//FMemMark MemMark(FMemStack::Get());
		FRDGBuilder GraphBuilder(RHICmdList);


		TRefCountPtr<IPooledRenderTarget> MaskTexCache, BorderTexCache, SDFTexCache, OcclusionTexCache;
		CacheRenderTarget(MaskResource->TextureRHI, TEXT("GenSDF_MaskTex"), MaskTexCache);
		CacheRenderTarget(BorderResource->TextureRHI, TEXT("GenSDF_BorderTex"), BorderTexCache);
		CacheRenderTarget(SDFResource->TextureRHI, TEXT("GenSDF_SDFTex"), SDFTexCache);
		CacheRenderTarget(DynamicOcclusionResource->TextureRHI, TEXT("GenSDF_OcclusionTex"), OcclusionTexCache);
			
			
		const FRDGTextureRef MaskTexRDG = GraphBuilder.RegisterExternalTexture(MaskTexCache);
		const FRDGTextureRef BorderTexRDG = GraphBuilder.RegisterExternalTexture(BorderTexCache);
		const FRDGTextureRef SDFTexRDG = GraphBuilder.RegisterExternalTexture(SDFTexCache);
		const FRDGTextureRef OcclusionTexRDG = GraphBuilder.RegisterExternalTexture(OcclusionTexCache);

		const FRDGTextureSRVRef MaskTexSRV = GraphBuilder.CreateSRV(MaskTexRDG);
		const FRDGTextureSRVRef BorderTexSRV = GraphBuilder.CreateSRV(BorderTexRDG);
		const FRDGTextureUAVRef SDFTexUAV = GraphBuilder.CreateUAV(SDFTexRDG);
		const FRDGTextureSRVRef OcclusionTexSRV = GraphBuilder.CreateSRV(OcclusionTexRDG);
			
		FRDGBufferUAVRef NearestPointBufferUAV;

		{
			const int32 NumBufferElements = SurfaceWidth * SurfaceHeight;
			FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector2f), NumBufferElements);
			BufferDesc.Usage |= BUF_SourceCopy | BUF_UnorderedAccess;
			const FRDGBufferRef NearestPointBuffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("GenSDF_NearestPointBuf"));
			NearestPointBufferUAV = GraphBuilder.CreateUAV(NearestPointBuffer);
		}

		const int32 ThreadGroupsPerSideX = FMath::DivideAndRoundUp(SurfaceWidth, 16);
		const int32 ThreadGroupsPerSideY = FMath::DivideAndRoundUp(SurfaceHeight, 16);
		const FIntVector ThreadGroups(ThreadGroupsPerSideX, ThreadGroupsPerSideY, 1);
			
		// Initialize the buffers
		{
			FDAJFAInitShader::FParameters* JFAParams = GraphBuilder.AllocParameters<FDAJFAInitShader::FParameters>();
			JFAParams->TextureWidth = SurfaceWidth;
			JFAParams->TextureHeight = SurfaceHeight;
			JFAParams->NearestPoint = NearestPointBufferUAV;
			JFAParams->MaskTexture = MaskTexSRV;
			JFAParams->BorderTexture = BorderTexSRV;
			JFAParams->OcclusionTexture = OcclusionTexSRV;
			JFAParams->SDFTexture = SDFTexUAV;

			const TShaderMapRef<FDAJFAInitShader> JFAShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("JFAInit"), JFAShader, JFAParams, ThreadGroups);
		}
	
		int JumpDistance = SurfaceWidth / 2;
		
		while (JumpDistance >= 1) {
			FDAJFAFindNearestPointShader::FParameters* JFAParams = GraphBuilder.AllocParameters<FDAJFAFindNearestPointShader::FParameters>();
			JFAParams->JumpDistance = JumpDistance;
			JFAParams->TextureWidth = SurfaceWidth;
			JFAParams->TextureHeight = SurfaceHeight;
			JFAParams->MaskTexture = MaskTexSRV;
			JFAParams->NearestPoint = NearestPointBufferUAV;

			const TShaderMapRef<FDAJFAFindNearestPointShader> JFAShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("JFANearestPoint"), JFAShader, JFAParams, ThreadGroups);

			JumpDistance /= 2;
		}

		// Write the result out to the SDF texture
		{
			FDAJFAWriteSDFShader::FParameters* JFAParams = GraphBuilder.AllocParameters<FDAJFAWriteSDFShader::FParameters>();
			JFAParams->TextureWidth = SurfaceWidth;
			JFAParams->TextureHeight = SurfaceHeight;
			JFAParams->MaxSDFValue = SurfaceWidth;
			JFAParams->MaskTexture = MaskTexSRV;
			JFAParams->NearestPoint = NearestPointBufferUAV;
			JFAParams->SDFTexture = SDFTexUAV;

			const TShaderMapRef<FDAJFAWriteSDFShader> JFAShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("JFAWriteSDF"), JFAShader, JFAParams, ThreadGroups);
		}
		
		GraphBuilder.Execute();
	});

	// Update the resources to regenerate the mips
	if (SDFTexture->bAutoGenerateMips) {
		SDFTexture->UpdateResourceImmediate(false);
	}
}

void UDungeonCanvasBlueprintFunctionLib::GenerateVoronoiSdfEffect(UTexture* SDFTexture, UTexture* BorderTexture,
		UCanvasRenderTarget2D* TargetEffectTexture, float ScaleMin, float ScaleMax)
{
	if (!SDFTexture || !TargetEffectTexture || !BorderTexture) {
		return;
	}
	if (TargetEffectTexture->GetSurfaceWidth() != SDFTexture->GetSurfaceWidth()
			|| TargetEffectTexture->GetSurfaceHeight() != SDFTexture->GetSurfaceHeight()) {
		return;
	}
	
	FTexture* SDFResource = SDFTexture->GetResource();
	FTexture* TargetResource = TargetEffectTexture->GetResource();
	FTexture* BorderResource = BorderTexture ? BorderTexture->GetResource() : nullptr;
	if (!SDFResource || !TargetResource) {
		return;
	}
	
	const int32 SurfaceWidth = SDFTexture->GetSurfaceWidth();
	const int32 SurfaceHeight = SDFTexture->GetSurfaceHeight();
	const FIntPoint TextureSize(SurfaceWidth, SurfaceHeight);

	
	TargetEffectTexture->Modify();
	ENQUEUE_RENDER_COMMAND(DAGenerateVoronoiEffect)([SDFResource, TargetResource, BorderResource, SurfaceWidth, SurfaceHeight, TextureSize](FRHICommandListImmediate& RHICmdList){
		FRDGBuilder GraphBuilder(RHICmdList);

		
		TRefCountPtr<IPooledRenderTarget> TargetTexCache, SDFTexCache, BorderTexCache, OcclusionTexCache;
		CacheRenderTarget(SDFResource->TextureRHI, TEXT("GenVoronoi_SDFTex"), SDFTexCache);
		CacheRenderTarget(TargetResource->TextureRHI, TEXT("GenVoronoi_TargetTex"), TargetTexCache);
		CacheRenderTarget(BorderResource->TextureRHI, TEXT("GenVoronoi_BorderTex"), BorderTexCache);
		CacheRenderTarget(GBlackTexture->TextureRHI, TEXT("GenVoronoi_OcclusionTex"), OcclusionTexCache);
		
		const FRDGTextureRef SDFTexRDG = GraphBuilder.RegisterExternalTexture(SDFTexCache);
		const FRDGTextureRef TargetTexRDG = GraphBuilder.RegisterExternalTexture(TargetTexCache);
		const FRDGTextureRef BorderTexRDG = GraphBuilder.RegisterExternalTexture(BorderTexCache);

		const FRDGTextureSRVRef SDFTexSRV = GraphBuilder.CreateSRV(SDFTexRDG);
		const FRDGTextureUAVRef TargetTexUAV = GraphBuilder.CreateUAV(TargetTexRDG);
		const FRDGTextureSRVRef TargetTexSRV = GraphBuilder.CreateSRV(TargetTexRDG);

		constexpr int NumCellsPerSide = 100;
		const int CellSize = FMath::RoundToInt(TargetResource->GetSizeX() / static_cast<float>(NumCellsPerSide));

		// Generate site points based on the sdf density
		TRefCountPtr<IPooledRenderTarget> SitePointTexture; 
		{
			const TCHAR* SitePointTextureName = TEXT("SitePointTexture");
			const FPooledRenderTargetDesc SitePointTexDesc = FPooledRenderTargetDesc::Create2DDesc(
			TextureSize,
			PF_R8,
			FClearValueBinding::None,
			TexCreate_None,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV,
			false);

			GRenderTargetPool.FindFreeElement(RHICmdList, SitePointTexDesc, SitePointTexture, SitePointTextureName);
		}
	
		const FRDGTextureRef SitePointTexRDG = GraphBuilder.RegisterExternalTexture(SitePointTexture);
		FRDGTextureSRVRef SitePointTexSRV = GraphBuilder.CreateSRV(SitePointTexRDG);
		FRDGTextureUAVRef SitePointTexUAV = GraphBuilder.CreateUAV(SitePointTexRDG);
		FRDGTextureSRVRef BorderTexSRV = GraphBuilder.CreateSRV(BorderTexRDG);
		
		AddClearRenderTargetPass(GraphBuilder, SitePointTexRDG, FLinearColor::Black);
		
		{
			FDACanvasVoronoiSDFShader::FParameters* EffectParams = GraphBuilder.AllocParameters<FDACanvasVoronoiSDFShader::FParameters>();
			EffectParams->TextureWidth = SurfaceWidth;
			EffectParams->TextureHeight = SurfaceHeight;
			EffectParams->CellSize = CellSize;
			EffectParams->TexSDF = SDFTexSRV;
			EffectParams->TexVoronoi = SitePointTexUAV;

			const int32 ThreadGroupsPerSideX = FMath::DivideAndRoundUp(NumCellsPerSide, 16);
			const int32 ThreadGroupsPerSideY = FMath::DivideAndRoundUp(NumCellsPerSide, 16);
			const FIntVector ThreadGroups(ThreadGroupsPerSideX, ThreadGroupsPerSideY, 1);
			
			const TShaderMapRef<FDACanvasVoronoiSDFShader> VoronoiSDFShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("VoronoiSDFGen"), VoronoiSDFShader, EffectParams, ThreadGroups);
		}

		const int32 ThreadGroupsPerSideX = FMath::DivideAndRoundUp(SurfaceWidth, 16);
		const int32 ThreadGroupsPerSideY = FMath::DivideAndRoundUp(SurfaceHeight, 16);
		const FIntVector ThreadGroups(ThreadGroupsPerSideX, ThreadGroupsPerSideY, 1);

		/*
		// Copy the border to the sites texture
		{
			TDABlitShader<EDABlurShaderOp::Add>::FParameters* BlitParams = GraphBuilder.AllocParameters<TDABlitShader<EDABlurShaderOp::Add>::FParameters>();
			BlitParams->TexSource = BorderTexSRV;
			BlitParams->TexDest = SitePointTexUAV;
			
			const TShaderMapRef<TDABlitShader<EDABlurShaderOp::Add>> BlitShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("VoronoiSiteBorderCopy"), BlitShader, BlitParams, ThreadGroups);
		}
		*/
		
		// Run JFA sdf shader on the site point texture, to create voronoi
		FRDGBufferUAVRef NearestPointBufferUAV;

		{
			const int32 NumBufferElements = SurfaceWidth * SurfaceHeight;
			FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector2f), NumBufferElements);
			BufferDesc.Usage |= BUF_SourceCopy | BUF_UnorderedAccess;
			const FRDGBufferRef NearestPointBuffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("GenSDF_NearestPointBuf"));
			NearestPointBufferUAV = GraphBuilder.CreateUAV(NearestPointBuffer);
		}
		
		// Initialize the buffers
		{
			FDAJFAInitShader::FParameters* JFAParams = GraphBuilder.AllocParameters<FDAJFAInitShader::FParameters>();
			JFAParams->TextureWidth = SurfaceWidth;
			JFAParams->TextureHeight = SurfaceHeight;
			JFAParams->NearestPoint = NearestPointBufferUAV;
			JFAParams->MaskTexture = SitePointTexSRV;
			JFAParams->BorderTexture = SitePointTexSRV;
			JFAParams->OcclusionTexture = SitePointTexSRV;
			JFAParams->SDFTexture = TargetTexUAV;

			const TShaderMapRef<FDAJFAInitShader> JFAShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("JFAInit"), JFAShader, JFAParams, ThreadGroups);
		}
	
		int JumpDistance = SurfaceWidth / 2;
		
		while (JumpDistance >= 1) {
			FDAJFAFindNearestPointShader::FParameters* JFAParams = GraphBuilder.AllocParameters<FDAJFAFindNearestPointShader::FParameters>();
			JFAParams->JumpDistance = JumpDistance;
			JFAParams->TextureWidth = SurfaceWidth;
			JFAParams->TextureHeight = SurfaceHeight;
			JFAParams->MaskTexture = SitePointTexSRV;
			JFAParams->NearestPoint = NearestPointBufferUAV;

			const TShaderMapRef<FDAJFAFindNearestPointShader> JFAShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("JFANearestPoint"), JFAShader, JFAParams, ThreadGroups);

			JumpDistance /= 2;
		}

		// Write the result out to the SDF texture
		{
			FDAJFAWriteSDFEdgeShader::FParameters* JFAParams = GraphBuilder.AllocParameters<FDAJFAWriteSDFEdgeShader::FParameters>();
			JFAParams->TextureWidth = SurfaceWidth;
			JFAParams->TextureHeight = SurfaceHeight;
			JFAParams->NearestPoint = NearestPointBufferUAV;
			JFAParams->SDFTexture = TargetTexUAV;

			const TShaderMapRef<FDAJFAWriteSDFEdgeShader> JFAShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("JFAWriteSDF"), JFAShader, JFAParams, ThreadGroups);
		}

		const int NumBlurIterations = 6;
		for (int i = 0; i < NumBlurIterations; i++) {
			// blur the sdf tex
			{
				FDAConvGaussBlur5x5Shader::FParameters* BlurParams = GraphBuilder.AllocParameters<FDAConvGaussBlur5x5Shader::FParameters>();
				BlurParams->TextureWidth = SurfaceWidth;
				BlurParams->TextureHeight = SurfaceHeight;
				BlurParams->InTexture = TargetTexSRV;
				BlurParams->OutTexture = SitePointTexUAV;

				const TShaderMapRef<FDAConvGaussBlur5x5Shader> BlurShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("VoroBlur"), BlurShader, BlurParams, ThreadGroups);
			}

			// Blit it back to the target texture
			{
				TDABlitShader<EDABlurShaderOp::Copy>::FParameters* BlitParams = GraphBuilder.AllocParameters<TDABlitShader<EDABlurShaderOp::Copy>::FParameters>();
				BlitParams->TexSource = SitePointTexSRV;
				BlitParams->TexDest = TargetTexUAV;
				const TShaderMapRef<TDABlitShader<EDABlurShaderOp::Copy>> BlitShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("VoroBlit"), BlitShader, BlitParams, ThreadGroups);
			}
		}
		
		GraphBuilder.Execute();
	});
	TargetEffectTexture->UpdateResourceImmediate(false);
	TargetEffectTexture->Modify();
}

void UDungeonCanvasBlueprintFunctionLib::UpdateDynamicOcclusions(const FDungeonCanvasDrawContext& DrawContext, UCanvasRenderTarget2D* DynamicOcclusionTexture,
			const FTransform& WorldBoundsTransform, const TArray<FDungeonCanvasOverlayIcon>& OverlayIcons)
{
	if (!DynamicOcclusionTexture || !DrawContext.DungeonCanvas) {
		return;
	}

	if (UWorld* World = DrawContext.DungeonCanvas->GetWorld()) {
		FVector2D CanvasSize;
		UCanvas* Canvas = nullptr;
		FDrawToRenderTargetContext RenderContext;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, DynamicOcclusionTexture, Canvas, CanvasSize, RenderContext);
		UKismetRenderingLibrary::ClearRenderTarget2D(World, DynamicOcclusionTexture, FLinearColor::Black);
		
		FDungeonCanvasViewportTransform View;
		View.SetLocalToWorld(WorldBoundsTransform);
		View.FocusOnCanvas(DynamicOcclusionTexture->SizeX, DynamicOcclusionTexture->SizeY);
		
		TMap<FName, FDungeonCanvasOverlayIcon> IconMap;
		for (const FDungeonCanvasOverlayIcon& OverlayIcon : OverlayIcons) {
			FDungeonCanvasOverlayIcon& IconRef = IconMap.FindOrAdd(OverlayIcon.Name);
			IconRef = OverlayIcon;
		}
		
		const TArray<FDungeonCanvasTrackedActorRegistryItem>& TrackedItems = DrawContext.DungeonCanvas->GetTrackedOverlayActors();
		for (const FDungeonCanvasTrackedActorRegistryItem& TrackedItem : TrackedItems) {
			const UDungeonCanvasTrackedObject* TrackedObject = TrackedItem.TrackedComponent.Get();
			if (TrackedObject && TrackedObject->bOccludesFogOfWar && IconMap.Contains(TrackedObject->IconName)) {
				FVector WorldLocation = TrackedObject->GetComponentLocation();
				const FVector2D CanvasLocation = View.WorldToCanvasLocation(WorldLocation);

				if (const FDungeonCanvasOverlayIcon* SearchResult = IconMap.Find(TrackedObject->IconName)) {
					const FDungeonCanvasOverlayIcon& OverlayData = *SearchResult;
					if (!OverlayData.ResourceObject) {
						continue;
					}
							
					float Rotation = OverlayData.RotationOffset;
					if (TrackedObject->bOrientToRotation) {
						const FVector Angles = TrackedObject->GetComponentToWorld().GetRotation().Euler();
						Rotation += Angles.Z;
					}
					float ScreenSize = OverlayData.ScreenSize;
					if (OverlayData.ScreenSizeType == EDungeonCanvasIconCoordinateSystem::Pixels) {
						ScreenSize = OverlayData.ScreenSize;
					}
					else if (OverlayData.ScreenSizeType == EDungeonCanvasIconCoordinateSystem::WorldCoordinates) {
						const FVector CanvasScale3D = View.GetWorldToCanvas().GetScale3D() * OverlayData.ScreenSize;
						ScreenSize = FMath::Max(CanvasScale3D.X, CanvasScale3D.Y);
					}

					DungeonCanvasBlueprintFunctionLib::DrawCanvasIcon(Canvas, OverlayData.ResourceObject, CanvasLocation, ScreenSize, OverlayData.AspectRatio, OverlayData.Tint, Rotation, BLEND_Translucent);
				}
			}
		}

		// Draw the doors
		TSet<const AActor*> DoorActors;
		for (const FDungeonCanvasTrackedActorRegistryItem& TrackedItem : TrackedItems) {
			if (const UDungeonCanvasTrackedObject* TrackedObject = TrackedItem.TrackedComponent.Get()) {
				if (const AActor* PossibleDoorActor = TrackedObject->GetOwner()) {
					if (PossibleDoorActor->GetClass()->ImplementsInterface(UDungeonDoorInterface::StaticClass())) {
						DoorActors.Add(PossibleDoorActor);
					}
				}
			}
		}

		for (const AActor* DoorActor : DoorActors) {
			if (DrawContext.DungeonCanvas->HeightRange.GetRangeType() != FDungeonCanvasHeightRangeType::EntireDungeon) {
				const float ActorZ = DoorActor->GetActorLocation().Z;
				const float MinHeight = DrawContext.DungeonCanvas->HeightRange.GetMinHeight();
				const float MaxHeight = DrawContext.DungeonCanvas->HeightRange.GetMaxHeight();
				if (ActorZ < MinHeight || ActorZ > MaxHeight) {
					continue;
				}
			}
			
			bool bIsOpen = IDungeonDoorInterface::Execute_IsPassageOpen(DoorActor);
			if (!bIsOpen) {
				// Draw a line covering the door
				float DoorWidth = IDungeonDoorInterface::Execute_GetDoorWidth(DoorActor);
				float BaseRotation = IDungeonDoorInterface::Execute_GetDoorBaseRotation(DoorActor);
				const FTransform& ActorTransform = DoorActor->GetActorTransform();
				FTransform Transform =
						FTransform(FRotator(0, BaseRotation, 0))
						* ActorTransform;
				
				FVector LocalDoorPointA(0, -DoorWidth * 0.5f, 0);
				FVector LocalDoorPointB(0, DoorWidth * 0.5f, 0);

				FVector WorldDoorPointA = Transform.TransformPosition(LocalDoorPointA);
				FVector WorldDoorPointB = Transform.TransformPosition(LocalDoorPointB);

				FVector2D CanvasDoorPointA = View.WorldToCanvasLocation(WorldDoorPointA);
				FVector2D CanvasDoorPointB = View.WorldToCanvasLocation(WorldDoorPointB);

				Canvas->K2_DrawLine(CanvasDoorPointA, CanvasDoorPointB, 2);
			}
		}
		
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, RenderContext);
		DynamicOcclusionTexture->Modify();
	}
}

void UDungeonCanvasBlueprintFunctionLib::BlurTexture3x(UCanvasRenderTarget2D* SourceTexture, UCanvasRenderTarget2D* DestinationTexture) {
	DungeonCanvasBlueprintFunctionLib::TBlurTexture<FDAConvGaussBlur3x3Shader>(SourceTexture, DestinationTexture);
}

void UDungeonCanvasBlueprintFunctionLib::BlurTexture5x(UCanvasRenderTarget2D* SourceTexture, UCanvasRenderTarget2D* DestinationTexture) {
	DungeonCanvasBlueprintFunctionLib::TBlurTexture<FDAConvGaussBlur5x5Shader>(SourceTexture, DestinationTexture);
}

void UDungeonCanvasBlueprintFunctionLib::DungeonCanvasDrawMaterial(const FDungeonCanvasDrawContext& DrawContext, UMaterialInterface* Material, const FTransform& WorldBoundsTransform) {
	UCanvas* Canvas = DrawContext.Canvas;
	const FDungeonCanvasViewportTransform& View = DrawContext.ViewTransform;

	if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material)) {
		SetCanvasMaterialWorldBounds(MID, WorldBoundsTransform);
#if WITH_EDITOR	
		if (DrawContext.bEditorPreviewMode) {
			static const FName ParamEditorPreviewMode("EditorPreviewMode");
			SetCanvasMaterialScalarParameter(MID, ParamEditorPreviewMode, 1);
		}
#endif
	}
	
	if (Material && Canvas && Canvas->Canvas) {
		// This is a user-facing function, so we'd rather make sure that shaders are ready by the time we render, in order to ensure we don't draw with a fallback material :
		Material->EnsureIsComplete();

		static const FVector2D QuadUV[] = {
			FVector2D(0, 0),
			FVector2D(1, 0),
			FVector2D(1, 1),
			FVector2D(0, 1),
		};
		
		const FVector WorldBoundsSize = WorldBoundsTransform.GetScale3D();
		const FVector WorldBoundsMin = WorldBoundsTransform.GetLocation() - WorldBoundsSize * 0.5f;
		const FVector WorldBoundsMax = WorldBoundsTransform.GetLocation() + WorldBoundsSize * 0.5f;

		const FVector2D Min = FVector2D(WorldBoundsMin);
		const FVector2D Max = FVector2D(WorldBoundsMax);
		const FVector2D WorldPoints[] = {
			Min,
			FVector2D(Max.X, Min.Y),
			Max,
			FVector2D(Min.X, Max.Y),
		};
		
		const FTransform WorldToCanvas = View.GetWorldToCanvas();
		TArray<FVector2D, TFixedAllocator<4>> QuadPoints;
		for (int i = 0; i < 4; i++) {
			FVector CanvasLocation = WorldToCanvas.TransformPosition(FVector(WorldPoints[i], 0));
			QuadPoints.Add(FVector2D(CanvasLocation));
		}

		auto DrawTriangle = [&](int I0, int I1, int I2) {
			FCanvasTriangleItem Triangle(
				QuadPoints[I0], QuadPoints[I1], QuadPoints[I2],
				QuadUV[I0], QuadUV[I1], QuadUV[I2],
				nullptr);
			Triangle.MaterialRenderProxy = Material->GetRenderProxy();
			Canvas->DrawItem(Triangle);
		};

		DrawTriangle(0, 1, 2);
		DrawTriangle(0, 2, 3);
	}
}

void UDungeonCanvasBlueprintFunctionLib::DungeonCanvasDrawLayoutIcons(const FDungeonCanvasDrawContext& DrawContext, const FDungeonLayoutData& DungeonLayout, const TArray<FDungeonCanvasOverlayIcon>& OverlayIcons, float OpacityMultiplier) {
	const ADungeonCanvas* DungeonCanvas = DrawContext.DungeonCanvas;
	if (!DungeonCanvas) {
		return;
	}
	
	if (UCanvas* Canvas = DrawContext.Canvas) {
		TMap<FName, FDungeonCanvasOverlayIcon> OverlayIconMap;
		for (const FDungeonCanvasOverlayIcon& OverlayIcon : OverlayIcons) {
			if (OverlayIcon.ResourceObject && !OverlayIconMap.Contains(OverlayIcon.Name)) {
				OverlayIconMap.Add(OverlayIcon.Name, OverlayIcon);
			}
		}

		const float MinHeight = DungeonCanvas->HeightRange.GetMinHeight();
		const float MaxHeight = DungeonCanvas->HeightRange.GetMaxHeight();
		const FDungeonLayoutData FilteredDungeonLayout = FDungeonLayoutUtils::FilterByHeightRange(DungeonLayout, MinHeight, MaxHeight); 

		const FDungeonCanvasViewportTransform& View = DrawContext.ViewTransform;
		const FTransform WorldToCanvas = View.GetWorldToCanvas();
		const float WorldToCanvasRotation = WorldToCanvas.GetRotation().Euler().Z;
		
		for (const FDungeonLayoutDataChunkInfo& ChunkShape : FilteredDungeonLayout.ChunkShapes) {
			for (const FDungeonPointOfInterest& PointOfInterest : ChunkShape.PointsOfInterest) {
				FVector Location = PointOfInterest.Transform.GetLocation();
				constexpr float EPSILON = 1e-4f;
				if (Location.Z + EPSILON >= MinHeight && Location.Z - EPSILON <= MaxHeight) {
					if (const FDungeonCanvasOverlayIcon* SearchResult = OverlayIconMap.Find(PointOfInterest.Id)) {
						const FDungeonCanvasOverlayIcon& OverlayIcon = *SearchResult;
						float Rotation = OverlayIcon.RotationOffset;
						if (!OverlayIcon.bAbsoluteRotation) {
							Rotation += WorldToCanvasRotation;
						}
						
						const FVector2D CanvasLocation = View.WorldToCanvasLocation(PointOfInterest.Transform.GetLocation());
						float ScreenSize = OverlayIcon.ScreenSize;
						if (OverlayIcon.ScreenSizeType == EDungeonCanvasIconCoordinateSystem::Pixels) {
							ScreenSize = OverlayIcon.ScreenSize;
						}
						else if (OverlayIcon.ScreenSizeType == EDungeonCanvasIconCoordinateSystem::WorldCoordinates) {
							FVector WorldToCanvasScale = View.GetWorldToCanvas().GetScale3D() * OverlayIcon.ScreenSize;
							ScreenSize = FMath::Max(WorldToCanvasScale.X, WorldToCanvasScale.Y);
						}

						FLinearColor Tint = OverlayIcon.Tint;
						Tint.A *= OpacityMultiplier;
						DungeonCanvasBlueprintFunctionLib::DrawCanvasIcon(Canvas, OverlayIcon.ResourceObject, CanvasLocation, ScreenSize, OverlayIcon.AspectRatio, Tint, Rotation, BLEND_Translucent);
					}
				}
			} 
		}
	}
}

void UDungeonCanvasBlueprintFunctionLib::DungeonCanvasDrawStairIcons(const FDungeonCanvasDrawContext& DrawContext,
		const FDungeonLayoutData& DungeonLayout, const FDungeonCanvasOverlayInternalIcon& InStairIcon, float OpacityMultiplier)
{
	const ADungeonCanvas* DungeonCanvas = DrawContext.DungeonCanvas;
	if (!DungeonCanvas) {
		return;
	}
	
	if (UCanvas* Canvas = DrawContext.Canvas) {
		const FDungeonCanvasViewportTransform& View = DrawContext.ViewTransform;
		const FTransform WorldToCanvas = View.GetWorldToCanvas();
		const float WorldToCanvasRotation = WorldToCanvas.GetRotation().Euler().Z;
		
		TMap<FName, FDungeonCanvasOverlayIcon> OverlayIconMap;
		const float MinHeight = DungeonCanvas->HeightRange.GetMinHeight();
		const float MaxHeight = DungeonCanvas->HeightRange.GetMaxHeight();
		const FDungeonLayoutData FilteredDungeonLayout = FDungeonLayoutUtils::FilterByHeightRange(DungeonLayout, MinHeight, MaxHeight);

		UObject* StairResourceObject = InStairIcon.ResourceObject.LoadSynchronous();
		
		for (const FDungeonLayoutDataStairItem& StairInfo : FilteredDungeonLayout.Stairs) {
			float Rotation = StairInfo.WorldTransform.GetRotation().Euler().Z - 90;
			if (!InStairIcon.bAbsoluteRotation) {
				Rotation += WorldToCanvasRotation;
			}
						
			const FVector2D CanvasLocation = View.WorldToCanvasLocation(StairInfo.WorldTransform.GetLocation());
			FLinearColor Tint = InStairIcon.Tint;
			Tint.A *= OpacityMultiplier;
			float LocalWorldSize = StairInfo.Width * InStairIcon.Scale;
			const FVector IconWorldToCanvasScale = View.GetWorldToCanvas().GetScale3D() * LocalWorldSize;
			float ScreenSize = FMath::Max(IconWorldToCanvasScale.X, IconWorldToCanvasScale.Y);
			
			DungeonCanvasBlueprintFunctionLib::DrawCanvasIcon(Canvas, StairResourceObject, CanvasLocation, ScreenSize, 1.0f, Tint, Rotation, BLEND_Translucent);
		}
	}
}

void UDungeonCanvasBlueprintFunctionLib::DungeonCanvasDrawTrackedActorIcons(
	const FDungeonCanvasDrawContext& DrawContext, const TArray<FDungeonCanvasOverlayIcon>& OverlayIcons, float OpacityMultiplier)
{
	const ADungeonCanvas* DungeonCanvas = DrawContext.DungeonCanvas;
	if (!DungeonCanvas) {
		return;
	}

	TArray<FDungeonCanvasTrackedActorRegistryItem> TrackedObjects = DungeonCanvas->GetTrackedOverlayActors();

	// Sort the z-order so important icons can render on top of everything else
	TrackedObjects.Sort([](const FDungeonCanvasTrackedActorRegistryItem& A, const FDungeonCanvasTrackedActorRegistryItem& B) {
		if (A.TrackedComponent.IsValid() && B.TrackedComponent.IsValid()) {
			return A.TrackedComponent->ZOrder < B.TrackedComponent->ZOrder;
		}
		return false;
	});

	TMap<FName, FDungeonCanvasOverlayIcon> OverlayIconMap;
	for (const FDungeonCanvasOverlayIcon& OverlayItem : OverlayIcons) {
		if (OverlayItem.ResourceObject && !OverlayIconMap.Contains(OverlayItem.Name)) {
			OverlayIconMap.Add(OverlayItem.Name, OverlayItem);
		}
	}

	if (UCanvas* Canvas = DrawContext.Canvas) {
		const FDungeonCanvasViewportTransform& View = DrawContext.ViewTransform;
		const FTransform WorldToCanvas = View.GetWorldToCanvas();
		const float WorldToCanvasRotation = WorldToCanvas.GetRotation().Euler().Z;
		for (const FDungeonCanvasTrackedActorRegistryItem& TrackedItem : TrackedObjects) {
			if (TrackedItem.TrackedComponent.IsValid()) {
				const UDungeonCanvasTrackedObject* TrackedObject = TrackedItem.TrackedComponent.Get();
				if (DungeonCanvas->HeightRange.GetRangeType() != FDungeonCanvasHeightRangeType::EntireDungeon) {
					const float ActorZ = TrackedObject->GetComponentLocation().Z;
					const float MinHeight = DungeonCanvas->HeightRange.GetMinHeight();
					const float MaxHeight = DungeonCanvas->HeightRange.GetMaxHeight();
					if (ActorZ < MinHeight || ActorZ > MaxHeight) {
						continue;
					} 
				}

				// Check if we need to hide the object based on its fog of war state
				if (TrackedItem.TrackedComponent->bHideWhenOutOfSight && DrawContext.DrawSettings.bFogOfWarEnabled) {
					constexpr float VisibilityThreshold = 0.01f;
					if (TrackedItem.FogOfWarLineOfSight < VisibilityThreshold || TrackedItem.FogOfWarExplored < VisibilityThreshold) {
						continue;
					}
				}
				
				FVector WorldLocation = TrackedObject->GetComponentLocation();
				FVector2D CanvasLocation = View.WorldToCanvasLocation(WorldLocation);

				if (FDungeonCanvasOverlayIcon* SearchResult = OverlayIconMap.Find(TrackedObject->IconName)) {
					const FDungeonCanvasOverlayIcon& OverlayData = *SearchResult;
					float Rotation = OverlayData.RotationOffset;
					if (TrackedObject->bOrientToRotation) {
						const FVector Angles = TrackedObject->GetComponentToWorld().GetRotation().Euler();
						Rotation += Angles.Z;
						Rotation += WorldToCanvasRotation;
					}
					float ScreenSize = OverlayData.ScreenSize;
					if (OverlayData.ScreenSizeType == EDungeonCanvasIconCoordinateSystem::Pixels) {
						ScreenSize = OverlayData.ScreenSize;
					}
					else if (OverlayData.ScreenSizeType == EDungeonCanvasIconCoordinateSystem::WorldCoordinates) {
						const FVector WorldToCanvasScale = View.GetWorldToCanvas().GetScale3D() * OverlayData.ScreenSize;
						ScreenSize = FMath::Max(WorldToCanvasScale.X, WorldToCanvasScale.Y);
					}
					
					FLinearColor Tint = OverlayData.Tint;
					Tint.A *= OpacityMultiplier;

					// Run this through a filter to modify the transform and looks
					FVector2D CanvasSize(Canvas->SizeX, Canvas->SizeY);
					TObjectPtr<UObject> ResourceObject = OverlayData.ResourceObject;
					for (UDungeonCanvasActorIconFilter* DrawFilter : DrawContext.DrawSettings.OverlayActorIconFilters) {
						if (DrawFilter) {
							DrawFilter->ApplyFilter(View, OverlayIconMap, OverlayData.Tags, ResourceObject, CanvasSize, CanvasLocation, ScreenSize, Tint, Rotation);
						}
					}
					
					DungeonCanvasBlueprintFunctionLib::DrawCanvasIcon(Canvas, ResourceObject, CanvasLocation, ScreenSize, OverlayData.AspectRatio, Tint, Rotation, BLEND_Translucent);
				}
			}
		}
	}
}

void UDungeonCanvasBlueprintFunctionLib::SetCanvasMaterialScalarParameter(UMaterialInstanceDynamic* Material, FName ParamName, float Value) {
	if (Material) {
		FMaterialLayersFunctions MaterialLayersFunctions;
		if (Material->GetMaterialLayers(MaterialLayersFunctions)) {
			// Set the global parameter
			Material->SetScalarParameterValue(ParamName, Value);

			// Try setting this parameter in each of the material layers
			const int32 NumLayers = MaterialLayersFunctions.Layers.Num();
			for (int LayerIdx = 0; LayerIdx < NumLayers; LayerIdx++) {
				FMaterialParameterInfo ParamInfo(ParamName, LayerParameter, LayerIdx);
				Material->SetScalarParameterValueByInfo(ParamInfo, Value);
			}
		}
	}
}

void UDungeonCanvasBlueprintFunctionLib::SetCanvasMaterialVectorParameter(UMaterialInstanceDynamic* Material, FName ParamName, FVector Value) {
	if (Material) {
		FMaterialLayersFunctions MaterialLayersFunctions;
		if (Material->GetMaterialLayers(MaterialLayersFunctions)) {
			// Set the global parameter
			Material->SetVectorParameterValue(ParamName, Value);

			// Try setting this parameter in each of the material layers
			const int32 NumLayers = MaterialLayersFunctions.Layers.Num();
			for (int LayerIdx = 0; LayerIdx < NumLayers; LayerIdx++) {
				FMaterialParameterInfo ParamInfo(ParamName, LayerParameter, LayerIdx);
				Material->SetVectorParameterValueByInfo(ParamInfo, Value);
			}
		}
	}
}

void UDungeonCanvasBlueprintFunctionLib::SetCanvasMaterialColorParameter(UMaterialInstanceDynamic* Material, FName ParamName, FLinearColor Value) {
	if (Material) {
		FMaterialLayersFunctions MaterialLayersFunctions;
		if (Material->GetMaterialLayers(MaterialLayersFunctions)) {
			// Set the global parameter
			Material->SetVectorParameterValue(ParamName, Value);

			// Try setting this parameter in each of the material layers
			const int32 NumLayers = MaterialLayersFunctions.Layers.Num();
			for (int LayerIdx = 0; LayerIdx < NumLayers; LayerIdx++) {
				FMaterialParameterInfo ParamInfo(ParamName, LayerParameter, LayerIdx);
				Material->SetVectorParameterValueByInfo(ParamInfo, Value);
			}
		}
	}
}

void UDungeonCanvasBlueprintFunctionLib::SetCanvasMaterialTextureParameter(UMaterialInstanceDynamic* Material, FName ParamName, UTexture* Value) {
	if (Material) {
		FMaterialLayersFunctions MaterialLayersFunctions;
		if (Material->GetMaterialLayers(MaterialLayersFunctions)) {
			// Set the global parameter
			Material->SetTextureParameterValue(ParamName, Value);

			// Try setting this parameter in each of the material layers
			const int32 NumLayers = MaterialLayersFunctions.Layers.Num();
			for (int LayerIdx = 0; LayerIdx < NumLayers; LayerIdx++) {
				FMaterialParameterInfo ParamInfo(ParamName, LayerParameter, LayerIdx);
				Material->SetTextureParameterValueByInfo(ParamInfo, Value);
			}
		}
	}
}

void UDungeonCanvasBlueprintFunctionLib::SetCanvasMaterialWorldBounds(UMaterialInstanceDynamic* Material, const FTransform& WorldBoundsTransform) {
	if (Material) {
		static const FName ParamWorldCenterX(TEXT("CanvasWorldCenterX"));
		static const FName ParamWorldCenterY(TEXT("CanvasWorldCenterY"));
		static const FName ParamWorldSize(TEXT("CanvasWorldSize"));
		const FVector WorldCenter = WorldBoundsTransform.GetLocation();
		const FVector WorldSize = WorldBoundsTransform.GetScale3D();
		SetCanvasMaterialScalarParameter(Material, ParamWorldCenterX, WorldCenter.X);
		SetCanvasMaterialScalarParameter(Material, ParamWorldCenterY, WorldCenter.Y);
		SetCanvasMaterialScalarParameter(Material, ParamWorldSize, FMath::Max(WorldSize.X, WorldSize.Y));
	}
}

void UDungeonCanvasBlueprintFunctionLib::UpdateFogOfWarTexture(const FDungeonCanvasDrawContext& DrawContext, UTexture* SDFTexture, const FTransform& WorldBoundsTransform)
{
	ADungeonCanvas* DungeonCanvas = DrawContext.DungeonCanvas;
	UCanvasRenderTarget2D* FogOfWarExploredTexture = DungeonCanvas ? DungeonCanvas->GetFogOfWarExploredTexture() : nullptr;
	UCanvasRenderTarget2D* FogOfWarVisibilityTexture = DungeonCanvas ? DungeonCanvas->GetFogOfWarVisibilityTexture() : nullptr;
	if (!DungeonCanvas || !FogOfWarExploredTexture || !FogOfWarVisibilityTexture) {
		return;
	}
	
	BeginFogOfWarUpdate(FogOfWarExploredTexture, FogOfWarVisibilityTexture);
	for (const ADungeonCanvas::FFogOfWarItemEntry& Explorer : DungeonCanvas->GetFogOfWarExplorers()) {
		if (Explorer.ActorPtr.IsValid()) {
			FVector WorldLocation = Explorer.ActorPtr->GetActorLocation();
			const FDungeonCanvasItemFogOfWarSettings& Settings = Explorer.Settings;
			const float HeightZ = WorldLocation.Z;
			FVector2D LightSource = FVector2D(WorldLocation);
			if (DungeonCanvas->InsideActiveFloorHeightRange(HeightZ)) {
				UpdateFogOfWarExplorer(FogOfWarExploredTexture, FogOfWarVisibilityTexture, SDFTexture, WorldBoundsTransform, LightSource, Settings.LightRadius, Settings.NumShadowSamples, Settings.ShadowJitterDistance);
			}
		}
	}
}


void UDungeonCanvasBlueprintFunctionLib::BeginFogOfWarUpdate(UCanvasRenderTarget2D* FogOfWarExploredTexture, UCanvasRenderTarget2D* FogOfWarVisibilityTexture) {
	if (!FogOfWarExploredTexture || !FogOfWarVisibilityTexture) {
		return;
	}
	if (FogOfWarExploredTexture->SizeX != FogOfWarVisibilityTexture->SizeX || FogOfWarExploredTexture->SizeY != FogOfWarVisibilityTexture->SizeY) {
		return;
	}
	
	const int32 SurfaceWidth = FogOfWarExploredTexture->SizeX;
	const int32 SurfaceHeight = FogOfWarExploredTexture->SizeY;

	if (SurfaceWidth == 0 || SurfaceHeight == 0) {
		return;
	}

	const FTextureResource* FoWVisibilityResource = FogOfWarVisibilityTexture->GetResource();

	const FIntVector ThreadGroupsFog = FIntVector(
			FMath::DivideAndRoundUp(SurfaceWidth, 16),
			FMath::DivideAndRoundUp(SurfaceHeight, 16),
			1);
	
	ENQUEUE_RENDER_COMMAND(DAUpdateFogOfWarInit)(
		[=](FRHICommandListImmediate& RHICmdList) {
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> FogTexVisibilityCache;
			CacheRenderTarget(FoWVisibilityResource->TextureRHI, TEXT("FoW_FogVisibilityTex"), FogTexVisibilityCache);

			const FRDGTextureRef FogTexVisibilityRDG = GraphBuilder.RegisterExternalTexture(FogTexVisibilityCache);

			FDACanvasFogOfWarInitFrameShader::FParameters* FogOfWarParams = GraphBuilder.AllocParameters<FDACanvasFogOfWarInitFrameShader::FParameters>();
			FogOfWarParams->TexFogOfWarVisibility = GraphBuilder.CreateUAV(FogTexVisibilityRDG);

			const TShaderMapRef<FDACanvasFogOfWarInitFrameShader> FogOfWarShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("CanvasFogOfWarInit"), FogOfWarShader, FogOfWarParams, ThreadGroupsFog);

			GraphBuilder.Execute();
		});

	FogOfWarExploredTexture->Modify();
	FogOfWarVisibilityTexture->Modify();
}

void UDungeonCanvasBlueprintFunctionLib::UpdateFogOfWarExplorer(UCanvasRenderTarget2D* FogOfWarExploredTexture, UCanvasRenderTarget2D* FogOfWarVisibilityTexture, UTexture* SDFTexture, const FTransform& WorldBoundsTransform,
			const FVector2D& LightSourceLocation, float LightRadius, float NumShadowSamples, int ShadowJitterDistance) {

	NumShadowSamples = FMath::Clamp(NumShadowSamples, 1, 10);

	int32 SurfaceWidth{}, SurfaceHeight{};
	{
		if (const UTextureRenderTarget2D* RTT = Cast<UTextureRenderTarget2D>(SDFTexture)) {
			SurfaceWidth = RTT->SizeX;
			SurfaceHeight = RTT->SizeY;
		}
		else if (const UTexture2D* Tex2D = Cast<UTexture2D>(SDFTexture)) {
			SurfaceWidth = Tex2D->GetSizeX();
			SurfaceHeight = Tex2D->GetSizeY();
		}
	}

	auto IsTexValid = [SurfaceWidth, SurfaceHeight](const UCanvasRenderTarget2D* Texture) {
		return Texture != nullptr && Texture->SizeX == SurfaceWidth && Texture->SizeY == SurfaceHeight; 
	};
	
	if (SurfaceWidth == 0 || SurfaceHeight == 0 || !IsTexValid(FogOfWarExploredTexture) || !IsTexValid(FogOfWarVisibilityTexture)) {
		return;
	}

	const FTextureResource* SDFResource = SDFTexture->GetResource();
	const FTextureResource* FoWExploredResource = FogOfWarExploredTexture->GetResource();
	const FTextureResource* FoWVisibilityResource = FogOfWarVisibilityTexture->GetResource();

	const FIntPoint TextureSize(SurfaceWidth, SurfaceHeight);

	const FVector WorldBoundsSize = WorldBoundsTransform.GetScale3D();
	const FVector WorldBoundsMin = WorldBoundsTransform.GetLocation() - WorldBoundsSize * 0.5f;
	const FVector WorldBoundsMax = WorldBoundsTransform.GetLocation() + WorldBoundsSize * 0.5f;
	
	const FVector2D LocationUV = (LightSourceLocation - FVector2D(WorldBoundsMin)) / FVector2D(WorldBoundsSize);
	auto Round2D = [](const FVector2D& V) {
		return FIntPoint(FMath::RoundToInt(V.X), FMath::RoundToInt(V.Y));
	};

	const FIntPoint FoWSourcePixel = Round2D(LocationUV * TextureSize);
	const float RadiusUV = LightRadius / WorldBoundsSize.X;
	const float RadiusPixels = RadiusUV * SurfaceWidth;

	const float SoftShadowRadiusPixels = ShadowJitterDistance / WorldBoundsSize.X * SurfaceWidth;

	const FIntPoint BaseOffset = FoWSourcePixel - FIntPoint(RadiusPixels, RadiusPixels);	
	const FIntPoint BaseSize = FIntPoint(RadiusPixels, RadiusPixels) * 2;

	const FIntVector ThreadGroupsFog = FIntVector(
			FMath::DivideAndRoundUp(BaseSize.X, 16),
			FMath::DivideAndRoundUp(BaseSize.Y, 16),
			1);

	ENQUEUE_RENDER_COMMAND(DAUpdateFogOfWar)(
		[=](FRHICommandListImmediate& RHICmdList) {
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> FogTexExploredCache, FogTexVisibilityCache, SDFTexCache;
			CacheRenderTarget(FoWExploredResource->TextureRHI, TEXT("FoW_FogExploredTex"), FogTexExploredCache);
			CacheRenderTarget(FoWVisibilityResource->TextureRHI, TEXT("FoW_FogVisibilityTex"), FogTexVisibilityCache);
			CacheRenderTarget(SDFResource->TextureRHI, TEXT("FoW_SDFTex"), SDFTexCache);
			
			const FRDGTextureRef FogTexExploredRDG = GraphBuilder.RegisterExternalTexture(FogTexExploredCache);
			const FRDGTextureRef FogTexVisibilityRDG = GraphBuilder.RegisterExternalTexture(FogTexVisibilityCache);
			const FRDGTextureRef SDFTexRDG = GraphBuilder.RegisterExternalTexture(SDFTexCache);

			FDACanvasFogOfWarShader::FParameters* FogOfWarParams = GraphBuilder.AllocParameters<FDACanvasFogOfWarShader::FParameters>();
			FogOfWarParams->FoWSourcePixel = FoWSourcePixel;
			FogOfWarParams->BaseOffset = BaseOffset;
			FogOfWarParams->RadiusPixels = RadiusPixels;

			FogOfWarParams->SoftShadowRadius = SoftShadowRadiusPixels;
			FogOfWarParams->SoftShadowSamples = NumShadowSamples;
			
			FogOfWarParams->TextureWidth = SurfaceWidth;
			FogOfWarParams->TextureHeight = SurfaceHeight;
			FogOfWarParams->TexSDF = GraphBuilder.CreateSRV(SDFTexRDG);
			FogOfWarParams->TexFogOfWarExplored = GraphBuilder.CreateUAV(FogTexExploredRDG);
			FogOfWarParams->TexFogOfWarVisibility = GraphBuilder.CreateUAV(FogTexVisibilityRDG);

			const TShaderMapRef<FDACanvasFogOfWarShader> FogOfWarShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("CanvasFogOfWar"), FogOfWarShader, FogOfWarParams, ThreadGroupsFog);

			GraphBuilder.Execute();
		});
	
	FogOfWarExploredTexture->Modify();
	FogOfWarVisibilityTexture->Modify();
}

