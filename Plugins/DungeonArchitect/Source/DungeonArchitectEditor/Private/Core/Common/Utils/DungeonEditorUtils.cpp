//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/Common/Utils/DungeonEditorUtils.h"

#include "Core/Dungeon.h"
#include "Core/Utils/DungeonConstants.h"
#include "Frameworks/Snap/Lib/SnapDungeonModelBase.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "EditorActorFolders.h"
#include "EditorViewportClient.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAssetViewport.h"
#include "ImageUtils.h"
#include "ObjectTools.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "DungeonEditorUtils"
DEFINE_LOG_CATEGORY(LogDungeonEditorUtils);

TObjectPtr<ADungeon> FDungeonEditorUtils::GetDungeonActorFromLevelViewport() {
    FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
    TSharedPtr<IAssetViewport> ViewportWindow = LevelEditorModule.GetFirstActiveViewport();
    TObjectPtr<ADungeon> DungeonCandidate = nullptr;
    if (ViewportWindow.IsValid()) {
        FEditorViewportClient& Viewport = ViewportWindow->GetAssetViewportClient();
        UWorld* World = Viewport.GetWorld();
        for (TActorIterator<ADungeon> DungeonIt(World); DungeonIt; ++DungeonIt) {
            ADungeon* Dungeon = *DungeonIt;
            if (Dungeon->IsSelected()) {
                return Dungeon;
            }
            DungeonCandidate = Dungeon;
        }
    }
    return DungeonCandidate;
}

void FDungeonEditorUtils::ShowNotification(
    FText Text, SNotificationItem::ECompletionState State /*= SNotificationItem::CS_Fail*/) {
    FNotificationInfo Info(Text);
    Info.bFireAndForget = true;
    Info.FadeOutDuration = 1.0f;
    Info.ExpireDuration = 2.0f;

    TWeakPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
    if (NotificationPtr.IsValid()) {
        NotificationPtr.Pin()->SetCompletionState(State);
    }
}

void FDungeonEditorUtils::SwitchToRealtimeMode() {
    FEditorViewportClient* ViewportClient = nullptr;
    if (GEditor) {
        if (const FViewport* ActiveViewport = GEditor->GetActiveViewport()) {
            ViewportClient = static_cast<FEditorViewportClient*>(ActiveViewport->GetClient());
        }
    }
    if (ViewportClient) {
        const bool bRealtime = ViewportClient->IsRealtime();
        if (!bRealtime) {
            ShowNotification(
                NSLOCTEXT("DungeonRealtimeMode", "DungeonRealtimeMode", "Switched viewport to Realtime mode"),
                SNotificationItem::CS_None);
            ViewportClient->SetRealtime(true);
        }
    }
    else {
        ShowNotification(NSLOCTEXT("ClientNotFound", "ClientNotFound", "Warning: Cannot find active viewport"));
    }
}

void FDungeonEditorUtils::CreateDungeonItemFolder(ADungeon* Dungeon) {
    if (Dungeon) {
        UWorld& World = *Dungeon->GetWorld();
        const FScopedTransaction Transaction(LOCTEXT("UndoAction_CreateFolder", "Create Folder"));

        auto& Folders = FActorFolders::Get();

        if (Dungeon->bUseCustomItemFolderName && !Dungeon->CustomItemFolderName.IsEmpty()) {
            Dungeon->ItemFolderPath = FName(Dungeon->CustomItemFolderName);
        }
        else {
            const FString FullPath = Dungeon->GetName() + "_Items";
            const FName Path(*FullPath);
            Dungeon->ItemFolderPath = Path;
        }

        const FFolder::FRootObject& RootObject(Dungeon);
        Folders.CreateFolder(World, FFolder(RootObject, Dungeon->ItemFolderPath));

        if (USnapDungeonModelBase* SnapModel = Cast<USnapDungeonModelBase>(Dungeon->GetModel())) {
            Folders.CreateFolder(World, FFolder(RootObject, FName(Dungeon->ItemFolderPath.ToString() + FDungeonArchitectConstants::DungeonFolderPathSuffix_SnapInstances)));
            Folders.CreateFolder(World, FFolder(RootObject, FName(Dungeon->ItemFolderPath.ToString() + FDungeonArchitectConstants::DungeonFolderPathSuffix_SnapConnections)));
        }
    }
    else {
        // Folder manager does not exist.  Clear the folder path from the dungeon actor,
        // so they are spawned in the root folder node
        Dungeon->ItemFolderPath = FName();
    }
}

void FDungeonEditorUtils::CollapseDungeonItemFolder(ADungeon* Dungeon) {
    if (Dungeon && Dungeon->GetWorld()) {
        const FFolder::FRootObject& RootObject(Dungeon);
        const FFolder DungeonFolder(RootObject, Dungeon->ItemFolderPath);

        // TODO: This does not work. Investigate
        FActorFolders::Get().SetIsFolderExpanded(*Dungeon->GetWorld(), DungeonFolder, false);
    }
}

void FDungeonEditorUtils::BuildDungeon(ADungeon* Dungeon) {
    SwitchToRealtimeMode();
    CreateDungeonItemFolder(Dungeon);
    Dungeon->BuildDungeon();
    CollapseDungeonItemFolder(Dungeon);
}

void FDungeonEditorUtils::HandleOpenURL(const TCHAR* URL) {
    FPlatformProcess::LaunchURL(URL, nullptr, nullptr);
}

void FDungeonEditorUtils::CaptureThumbnailFromViewport(FViewport* InViewport, const TArray<FAssetData>& InAssetsToAssign, const TFunction<FColor(const FColor&)>& InColorTransform) {
    //capture the thumbnail
	uint32 SrcWidth = InViewport->GetSizeXY().X;
	uint32 SrcHeight = InViewport->GetSizeXY().Y;
	// Read the contents of the viewport into an array.
	TArray<FColor> OrigBitmap;
	if (InViewport->ReadPixels(OrigBitmap))
	{
		check(OrigBitmap.Num() == SrcWidth * SrcHeight);

		//pin to smallest value
		int32 CropSize = FMath::Min<uint32>(SrcWidth, SrcHeight);
		//pin to max size
		int32 ScaledSize  = FMath::Min<uint32>(ThumbnailTools::DefaultThumbnailSize, CropSize);

		//calculations for cropping
		TArray<FColor> CroppedBitmap;
		CroppedBitmap.AddUninitialized(CropSize*CropSize);
		//Crop the image
		int32 CroppedSrcTop  = (SrcHeight - CropSize)/2;
		int32 CroppedSrcLeft = (SrcWidth - CropSize)/2;
		for (int32 Row = 0; Row < CropSize; ++Row)
		{
			//Row*Side of a row*byte per color
			int32 SrcPixelIndex = (CroppedSrcTop+Row)*SrcWidth + CroppedSrcLeft;
			const void* SrcPtr = &(OrigBitmap[SrcPixelIndex]);
			void* DstPtr = &(CroppedBitmap[Row*CropSize]);
			FMemory::Memcpy(DstPtr, SrcPtr, CropSize*4);
		}

		//Scale image down if needed
		TArray<FColor> ScaledBitmap;
		if (ScaledSize < CropSize)
		{
			FImageUtils::ImageResize( CropSize, CropSize, CroppedBitmap, ScaledSize, ScaledSize, ScaledBitmap, true );
		}
		else
		{
			//just copy the data over. sizes are the same
			ScaledBitmap = CroppedBitmap;
		}

		// Transform the color
		for (FColor& Color : ScaledBitmap) {
			Color = InColorTransform(Color);
		} 
		
		//setup actual thumbnail
		FObjectThumbnail TempThumbnail;
		TempThumbnail.SetImageSize( ScaledSize, ScaledSize );
		TArray<uint8>& ThumbnailByteArray = TempThumbnail.AccessImageData();

		// Copy scaled image into destination thumb
		int32 MemorySize = ScaledSize*ScaledSize*sizeof(FColor);
		ThumbnailByteArray.AddUninitialized(MemorySize);
		FMemory::Memcpy(&(ThumbnailByteArray[0]), &(ScaledBitmap[0]), MemorySize);

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

		//check if each asset should receive the new thumb nail
		for ( auto AssetIt = InAssetsToAssign.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			const FAssetData& CurrentAsset = *AssetIt;

			//assign the thumbnail and dirty
			const FString ObjectFullName = CurrentAsset.GetFullName();
			const FString PackageName    = CurrentAsset.PackageName.ToString();

			UPackage* AssetPackage = FindObject<UPackage>( NULL, *PackageName );
			if ( ensure(AssetPackage) )
			{
				FObjectThumbnail* NewThumbnail = ThumbnailTools::CacheThumbnail(ObjectFullName, &TempThumbnail, AssetPackage);
				if ( ensure(NewThumbnail) )
				{
					//we need to indicate that the package needs to be resaved
					AssetPackage->MarkPackageDirty();

					// Let the content browser know that we've changed the thumbnail
					NewThumbnail->MarkAsDirty();
						
					// Signal that the asset was changed if it is loaded so thumbnail pools will update
					if ( CurrentAsset.IsAssetLoaded() )
					{
						CurrentAsset.GetAsset()->PostEditChange();
					}

					//Set that thumbnail as a valid custom thumbnail so it'll be saved out
					NewThumbnail->SetCreatedAfterCustomThumbsEnabled();
				}
			}
		}
	}
}

void FAssetPackageInfo::AddReferencedObjects(FReferenceCollector& Collector) {
    Collector.AddReferencedObject(Asset);
    Collector.AddReferencedObject(Package);
}

FString FAssetPackageInfo::GetReferencerName() const {
    static const FString NameString = TEXT("FAssetPackageInfo");
    return NameString;
}

FAssetPackageInfo FDungeonAssetUtils::DuplicateAsset(UObject* SourceAsset, const FString& TargetPackageName,
                                                     const FString& TargetObjectName) {
    FAssetPackageInfo Duplicate;
    if (SourceAsset) {
        // Make sure the referenced object is deselected before duplicating it.
        GEditor->GetSelectedObjects()->Deselect(SourceAsset);

        // Duplicate the asset
        Duplicate.Package = CreatePackage(*TargetPackageName);
        Duplicate.Asset = StaticDuplicateObject(SourceAsset, Duplicate.Package, *TargetObjectName);

        if (Duplicate.Asset) {
            Duplicate.Asset->MarkPackageDirty();

            // Notify the asset registry
            FAssetRegistryModule::AssetCreated(Duplicate.Asset);
        }
        else {
            UE_LOG(LogDungeonEditorUtils, Error, TEXT("Failed to duplicate asset %s"), *TargetObjectName);
        }
    }

    return Duplicate;
}

void FDungeonAssetUtils::SaveAsset(const FAssetPackageInfo& Info) {
    if (Info.Asset && Info.Package) {
        Info.Package->SetDirtyFlag(true);
        const FString PackagePath = Info.Package->GetOutermost()->GetName();
        const FString Filename = *FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        SaveArgs.bForceByteSwapping = false;
        SaveArgs.bWarnOfLongFilename = true;
        SaveArgs.Error = GError;

        if (!UPackage::SavePackage(Info.Package, nullptr, *Filename, SaveArgs)) {
            UE_LOG(LogDungeonEditorUtils, Display, TEXT("Unable to save asset %s"), *Info.Asset->GetName());
        }
    }
}


#undef LOCTEXT_NAMESPACE

