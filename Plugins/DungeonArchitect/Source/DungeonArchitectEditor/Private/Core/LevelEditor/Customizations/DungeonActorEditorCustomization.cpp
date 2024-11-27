//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Core/LevelEditor/Customizations/DungeonActorEditorCustomization.h"

#include "Builders/CellFlow/CellFlowBuilder.h"
#include "Builders/FloorPlan/FloorPlanBuilder.h"
#include "Builders/Grid/GridDungeonBuilder.h"
#include "Builders/GridFlow/GridFlowBuilder.h"
#include "Builders/SimpleCity/SimpleCityBuilder.h"
#include "Builders/SnapGridFlow/SnapGridFlowDungeon.h"
#include "Builders/SnapGridFlow/SnapGridFlowEditorUtils.h"
#include "Builders/SnapMap/SnapMapDungeonBuilder.h"
#include "Builders/SnapMap/Utils/SnapMapModuleDBUtils.h"
#include "Core/Common/Utils/DungeonEditorConstants.h"
#include "Core/Common/Utils/DungeonEditorUtils.h"
#include "Core/Dungeon.h"
#include "Core/Editors/ThemeEditor/AppModes/ThemeGraph/Graph/EdGraphNode_DungeonMesh.h"
#include "Core/LevelEditor/Customizations/DungeonArchitectStyle.h"
#include "Core/LevelEditor/HelpSystem/DungeonArchitectHelpSystem.h"
#include "Core/Utils/Debug/DungeonDebug.h"
#include "Core/Utils/DungeonEditorViewportProperties.h"
#include "Core/Volumes/DungeonVolume.h"
#include "Frameworks/Canvas/DungeonCanvas.h"
#include "Frameworks/Canvas/DungeonCanvasBlueprint.h"
#include "Frameworks/GraphGrammar/ExecutionGraph/Nodes/EdGraphNode_GrammarExecRuleNode.h"
#include "Frameworks/LevelStreaming/DungeonLevelStreamer.h"
#include "Frameworks/MarkerGenerator/MarkerGenPattern.h"
#include "Frameworks/Snap/Lib/Module/SnapModuleBoundShape.h"
#include "Frameworks/Snap/SnapGridFlow/SnapGridFlowModuleDatabase.h"
#include "Frameworks/Snap/SnapMap/SnapMapModuleDatabase.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Dialogs/DlgPickPath.h"
#include "IContentBrowserSingleton.h"
#include "IDocumentation.h"
#include "Interfaces/IMainFrameModule.h"
#include "Internationalization/Regex.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ObjectEditorUtils.h"
#include "PropertyCustomizationHelpers.h"
#include "Selection.h"
#include "UObject/Class.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "DungeonArchitectEditorModule"

DEFINE_LOG_CATEGORY(LogDungeonCustomization)

namespace DACustomize {
    ADungeon* GetDungeon(IDetailLayoutBuilder* DetailBuilder) {
        return FDungeonEditorUtils::GetBuilderObject<ADungeon>(DetailBuilder);
    }

    void ShowDungeonConfigProperties(IDetailLayoutBuilder& DetailBuilder, UDungeonConfig* Config) {
        if (!Config) return;

        TArray<FName> ImportantAttributes;
        ImportantAttributes.Add("Seed");

        TArray<UObject*> Configs;
        Configs.Add(Config);
    
        const FText ConfigName = Config->GetClass()->GetDisplayNameText();
        IDetailCategoryBuilder& ConfigCategory = DetailBuilder.EditCategory(*ConfigName.ToString(), FText::GetEmpty(), ECategoryPriority::Important);

        // First add the important attributes, so they show up on top of the config properties list
        for (const FName& Attrib : ImportantAttributes) {
            ConfigCategory.AddExternalObjectProperty(Configs, Attrib);
        }

        TArray<FProperty*> ExperimentalProperties;

        for (TFieldIterator<FProperty> PropIt(Config->GetClass()); PropIt; ++PropIt) {
            FProperty* Property = *PropIt;

            if (Property->HasMetaData("Experimental")) {
                ExperimentalProperties.Add(Property);
                continue;
            }

            FName PropertyName(*(Property->GetName()));

            // Make sure we skip the important attrib, since we have already added them above
            if (ImportantAttributes.Contains(PropertyName)) {
                // Already added
                continue;
            }

            ConfigCategory.AddExternalObjectProperty(Configs, PropertyName);
        }

        IDetailCategoryBuilder& ExperimentalCategory = DetailBuilder.EditCategory("Experimental");
        for (FProperty* Property : ExperimentalProperties) {
            const FName PropertyName(*(Property->GetName()));
            ExperimentalCategory.AddExternalObjectProperty(Configs, PropertyName);
        }
    }

    void NotifyPropertyChanged(UObject* InObject, const FName& InFieldName) {
        if (InObject) {
            if (FProperty* BuilderClassProperty = FindFProperty<FProperty>(InObject->GetClass(), InFieldName)) {
                InObject->PreEditChange(BuilderClassProperty);

                FPropertyChangedEvent PropertyEvent(BuilderClassProperty);
                InObject->PostEditChangeProperty(PropertyEvent);
            }
        }
    }

    void ShowLevelStreamingProperties(IDetailLayoutBuilder& DetailBuilder, FDungeonLevelStreamingConfig* StreamingConfig) {
        const FString ConfigName = "Level Streaming";
        IDetailCategoryBuilder& ConfigCategory = DetailBuilder.EditCategory(*ConfigName);

        const TSharedRef<FStructOnScope> ConfigStructRef = MakeShared<FStructOnScope>(
            FDungeonLevelStreamingConfig::StaticStruct(), reinterpret_cast<uint8*>(StreamingConfig));
        ConfigCategory.AddAllExternalStructureProperties(ConfigStructRef);
    }
}

void SDungeonBuilderClassCombo::Construct(const FArguments& InArgs) {
    OnSetDungeonBuilder = InArgs._OnSetDungeonBuilder;
    SelectedBuilderItem = InArgs._SelectedBuilderItem;
    
    RegenerateBuilderComboItems();
    
    SComboBox<UClass*>::Construct(
            typename SComboBox<UClass*>::FArguments()
            .OptionsSource(&BuilderClassComboItems)
            .OnComboBoxOpening(this, &SDungeonBuilderClassCombo::RegenerateBuilderComboItems)
            .OnGenerateWidget(this, &SDungeonBuilderClassCombo::GenerateBuilderClassComboRowItem)
            .OnSelectionChanged_Lambda([this](const TSubclassOf<UDungeonBuilder>& InItem, ESelectInfo::Type InSelectType)
            {
                if (InItem) {
                    SelectedBuilderItem = InItem;
                }
                
                // This is for the initial selection.
                if (InSelectType == ESelectInfo::Direct) {
                    return;
                }

                if (OnSetDungeonBuilder.IsBound()) {
                    OnSetDungeonBuilder.Execute(InItem);
                }
                
            })
            .Content()
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText {
                    return SelectedBuilderItem
                        ? SelectedBuilderItem->GetDisplayNameText()
                        : LOCTEXT("BuilderClass_DefaultNullText", "[None]");
                })
            ]
        );
}

TSharedRef<SWidget> SDungeonBuilderClassCombo::GenerateBuilderClassComboRowItem(UClass* InBuilderClass) const {
    if (!InBuilderClass) {
        return SNullWidget::NullWidget;
    }
    const FText BuilderClassText = InBuilderClass ? InBuilderClass->GetDisplayNameText() : FText::GetEmpty();

    TSharedPtr<SBox> TitlePillContainer;
    
    const TSharedRef<SVerticalBox> ItemWidget = SNew(SVerticalBox)
        +SVerticalBox::Slot()
        .Padding(2)
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text(BuilderClassText)
                .AutoWrapText(true)
                .TextStyle(FDungeonArchitectStyle::Get(), "DungeonActor.BuilderClass.Title")
            ]
            +SHorizontalBox::Slot()
            .Padding(4, 0, 0, 0)
            .AutoWidth()
            [
                SAssignNew(TitlePillContainer, SBox)
            ]
        ];

    FText Description = InBuilderClass->GetMetaDataText("Description");
    if (Description.IsEmptyOrWhitespace()) {
        Description = LOCTEXT("UserDefinedBuilderDesc", "User-defined Builder");
    }

    // Add the description slot
    {
        ItemWidget->AddSlot()
        .Padding(8, 4, 4, 4)
        .AutoHeight()
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text(Description)
        ];
    }

    bool bIsExperimental = false;
    bool bIsEarlyAccess = false;
    {
        FString MostDerivedClassName;
        FObjectEditorUtils::GetClassDevelopmentStatus(InBuilderClass, bIsExperimental, bIsEarlyAccess, MostDerivedClassName);
    }

    const bool bSelected = SelectedBuilderItem && SelectedBuilderItem == InBuilderClass;
    const FLinearColor BackgroundColor = bSelected
        ? FLinearColor(0.25, 1, 0.25, 0.8f)
        : FLinearColor(1, 1, 1, 0.4f);

    const TSharedRef<SWidget> HostWidget = SNew(SBorder)
       .BorderImage(FDungeonArchitectStyle::Get().GetBrush("DungeonActor.BuilderClass.ItemBorder"))
       .BorderBackgroundColor(BackgroundColor)
       .Padding(0.0f)
       [
            ItemWidget
       ];

    if (bIsExperimental || bIsEarlyAccess) {
        constexpr float Alpha = 0.25f;
        const FLinearColor PillColor = bIsEarlyAccess
                ? FLinearColor(0.25f, 1, 0, Alpha)
                : FLinearColor(1, 0.5, 0, Alpha);

        const FText PillText = bIsExperimental
            ? LOCTEXT("PillText_Experimental", "Experimental")
            : LOCTEXT("PillText_EarlyAccess", "Early Access");

        TitlePillContainer->SetContent(
            SNew(SBox)
            .VAlign(VAlign_Center)
            [
                SNew(SBorder)
                .BorderImage(FDungeonArchitectStyle::Get().GetBrush("DungeonArchitect.RoundDarkBorder"))
                .Padding(4)
                .BorderBackgroundColor(PillColor)
                [
                    SNew(STextBlock)
                    .Text(PillText)
                ]
            ]);
    }
    
    return SNew(SBox)
    .MaxDesiredWidth(400)
    [
        HostWidget
    ];
}

void SDungeonBuilderClassCombo::RegenerateBuilderComboItems() {
    static const TArray<UClass*> CustomBuilderInfo = {
        UGridFlowBuilder::StaticClass(),
        USnapGridFlowBuilder::StaticClass(),
        UCellFlowBuilder::StaticClass(),
        USnapMapDungeonBuilder::StaticClass(),
        USimpleCityBuilder::StaticClass(),
        UGridDungeonBuilder::StaticClass(),
        UFloorPlanBuilder::StaticClass(),
    };
    
    TSet<TObjectPtr<UClass>> BuilderClassesToProcess;
    for (const TSubclassOf<UDungeonBuilder>& BuilderClass : CustomBuilderInfo) {
        if (BuilderClass) {
            BuilderClassesToProcess.Add(BuilderClass);
        }
    }

    {
        const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();
        TSet<FTopLevelAssetPath> InheritedBuilderPaths;
        AssetRegistry.GetDerivedClassNames({UDungeonBuilder::StaticClass()->GetClassPathName()}, {}, InheritedBuilderPaths);

        for (const FTopLevelAssetPath& InheritedBuilderPath : InheritedBuilderPaths) {
            TSoftObjectPtr<UClass> SoftObjectPtr(InheritedBuilderPath.ToString());
            if (UClass* BuilderClass = SoftObjectPtr.LoadSynchronous())
            {
                if (BuilderClassesToProcess.Contains(BuilderClass)) {
                    // Already processed
                    continue;
                }
                
                BuilderClassesToProcess.Add(BuilderClass);
            }
        }
    }

    BuilderClassComboItems.Reset();
    for (UClass* BuilderClass : BuilderClassesToProcess) {
        if (BuilderClass->HasAnyClassFlags(CLASS_Abstract)) continue;
        if (BuilderClass->HasAnyClassFlags(CLASS_HideDropDown)) continue;
        if (BuilderClass->GetName().StartsWith(TEXT("SKEL_")) || BuilderClass->GetName().StartsWith(TEXT("REINST_"))) {
            continue;
        }
        
        BuilderClassComboItems.Add(BuilderClass);
    }
}

///////////////////////////// FDungeonBuilderClassCombo /////////////////////////////
FDungeonBuilderClassCombo::FDungeonBuilderClassCombo(const FnGetBuilder& InGetBuilder, const FnSetBuilder& InSetBuilder)
    : GetBuilder(InGetBuilder)
    , SetBuilder(InSetBuilder)
{
    RegenerateBuilderComboItems();
    
    SelectedBuilderItem = {};

    if (const TSubclassOf<UDungeonBuilder> BuilderClass = GetBuilder()) {
        SelectedBuilderItem = GetItemEntry(BuilderClass);
    }
}

TSharedPtr<FDungeonBuilderClassCombo::FBuilderClassEntry> FDungeonBuilderClassCombo::GetItemEntry(TSubclassOf<UDungeonBuilder> InBuilderClass) {
    for (const TSharedPtr<FBuilderClassEntry>& BuilderClassItem : BuilderClassComboItems) {
        if (BuilderClassItem.IsValid() && BuilderClassItem->BuilderClass == InBuilderClass) {
            return BuilderClassItem;
        }
    }

    if (!SelectedBuilderItem.IsValid() && InBuilderClass) {
        return MakeShareable(new FBuilderClassEntry(InBuilderClass));
    }
    return nullptr;
}

void FDungeonBuilderClassCombo::RegenerateBuilderComboItems() {
    static const TArray<TSharedPtr<FBuilderClassEntry>> CustomBuilderInfo = {
        MakeShareable(new FBuilderClassEntry(UGridFlowBuilder::StaticClass())),
        MakeShareable(new FBuilderClassEntry(USnapGridFlowBuilder::StaticClass())),
        MakeShareable(new FBuilderClassEntry(UCellFlowBuilder::StaticClass())),
        MakeShareable(new FBuilderClassEntry(USnapMapDungeonBuilder::StaticClass())),
        MakeShareable(new FBuilderClassEntry(USimpleCityBuilder::StaticClass())),
        MakeShareable(new FBuilderClassEntry(UGridDungeonBuilder::StaticClass())),
        MakeShareable(new FBuilderClassEntry(UFloorPlanBuilder::StaticClass())),
    };

    TSet<TObjectPtr<UClass>> BuilderClassesToProcess;
    for (const TSharedPtr<FBuilderClassEntry>& CustomEntry : CustomBuilderInfo) {
        if (CustomEntry.IsValid()) {
            BuilderClassesToProcess.Add(CustomEntry->BuilderClass);
        }
    }

    {
        const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();
        TSet<FTopLevelAssetPath> InheritedBuilderPaths;
        AssetRegistry.GetDerivedClassNames({UDungeonBuilder::StaticClass()->GetClassPathName()}, {}, InheritedBuilderPaths);

        for (const FTopLevelAssetPath& InheritedBuilderPath : InheritedBuilderPaths) {
            TSoftObjectPtr<UClass> SoftObjectPtr(InheritedBuilderPath.ToString());
            if (UClass* BuilderClass = SoftObjectPtr.LoadSynchronous())
            {
                if (BuilderClassesToProcess.Contains(BuilderClass)) {
                    // Already processed
                    continue;
                }
                
                BuilderClassesToProcess.Add(BuilderClass);
            }
        }
    }

    BuilderClassComboItems.Reset();
    for (UClass* BuilderClass : BuilderClassesToProcess) {
        if (BuilderClass->HasAnyClassFlags(CLASS_Abstract)) continue;
        if (BuilderClass->HasAnyClassFlags(CLASS_HideDropDown)) continue;
        if (BuilderClass->GetName().StartsWith(TEXT("SKEL_")) || BuilderClass->GetName().StartsWith(TEXT("REINST_"))) {
            continue;
        }
        
        BuilderClassComboItems.Add(MakeShareable(new FBuilderClassEntry(BuilderClass)));
    }
}

void FDungeonBuilderClassCombo::UseSelectedBuilderClass() const {
    FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
    USelection* AssetSelection = GEditor->GetSelectedObjects();
    if (AssetSelection && AssetSelection->Num() == 1) {
        if (UObject* ObjectToAssign = AssetSelection->GetTop<UObject>()) {
            UClass* BuilderClassToAssign = Cast<UClass>(ObjectToAssign);
            if (!BuilderClassToAssign) {
                if (const UBlueprint* BuilderBlueprint = Cast<UBlueprint>(ObjectToAssign)) {
                    BuilderClassToAssign = BuilderBlueprint->GeneratedClass;
                }
            }

            if (BuilderClassToAssign && BuilderClassToAssign->IsChildOf(UDungeonBuilder::StaticClass()) && !BuilderClassToAssign->HasAnyClassFlags(CLASS_Abstract)) {
                SetBuilder(BuilderClassToAssign);
            }
        }
    }
}

void FDungeonBuilderClassCombo::OnBrowseToSelectedBuilderClass() const {
    if (const TSubclassOf<UDungeonBuilder> BuilderClass = GetBuilder()) {
        TArray<UObject*> Objects;
        Objects.Add(BuilderClass);
        GEditor->SyncBrowserToObjects(Objects);
    }
}

TSharedRef<SWidget> FDungeonBuilderClassCombo::GenerateBuilderClassComboRowItem(TSharedPtr<FBuilderClassEntry> Item) const {
    if (!Item.IsValid() || !Item->BuilderClass) {
        return SNullWidget::NullWidget;
    }
    const FText BuilderClassText = Item->BuilderClass ? Item->BuilderClass->GetDisplayNameText() : FText::GetEmpty();

    TSharedPtr<SBox> TitlePillContainer;
    
    const TSharedRef<SVerticalBox> ItemWidget = SNew(SVerticalBox)
        +SVerticalBox::Slot()
        .Padding(2)
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text(BuilderClassText)
                .AutoWrapText(true)
                .TextStyle(FDungeonArchitectStyle::Get(), "DungeonActor.BuilderClass.Title")
            ]
            +SHorizontalBox::Slot()
            .Padding(4, 0, 0, 0)
            .AutoWidth()
            [
                SAssignNew(TitlePillContainer, SBox)
            ]
        ];

    FText Description = Item->BuilderClass->GetMetaDataText("Description");
    if (Description.IsEmptyOrWhitespace()) {
        Description = LOCTEXT("UserDefinedBuilderDesc", "User-defined Builder");
    }

    // Add the description slot
    {
        ItemWidget->AddSlot()
        .Padding(8, 4, 4, 4)
        .AutoHeight()
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text(Description)
        ];
    }

    bool bIsExperimental = false;
    bool bIsEarlyAccess = false;
    {
        FString MostDerivedClassName;
        FObjectEditorUtils::GetClassDevelopmentStatus(Item->BuilderClass, bIsExperimental, bIsEarlyAccess, MostDerivedClassName);
    }

    const bool bSelected = SelectedBuilderItem.IsValid() && SelectedBuilderItem->BuilderClass == Item->BuilderClass;
    const FLinearColor BackgroundColor = bSelected
        ? FLinearColor(0.25, 1, 0.25, 0.8f)
        : FLinearColor(1, 1, 1, 0.4f);

    const TSharedRef<SWidget> HostWidget = SNew(SBorder)
       .BorderImage(FDungeonArchitectStyle::Get().GetBrush("DungeonActor.BuilderClass.ItemBorder"))
       .BorderBackgroundColor(BackgroundColor)
       .Padding(0.0f)
       [
            ItemWidget
       ];

    if (bIsExperimental || bIsEarlyAccess) {
        constexpr float Alpha = 0.25f;
        const FLinearColor PillColor = bIsEarlyAccess
                ? FLinearColor(0.25f, 1, 0, Alpha)
                : FLinearColor(1, 0.5, 0, Alpha);

        const FText PillText = bIsExperimental
            ? LOCTEXT("PillText_Experimental", "Experimental")
            : LOCTEXT("PillText_EarlyAccess", "Early Access");

        TitlePillContainer->SetContent(
            SNew(SBox)
            .VAlign(VAlign_Center)
            [
                SNew(SBorder)
                .BorderImage(FDungeonArchitectStyle::Get().GetBrush("DungeonArchitect.RoundDarkBorder"))
                .Padding(4)
                .BorderBackgroundColor(PillColor)
                [
                    SNew(STextBlock)
                    .Text(PillText)
                ]
            ]);
    }
    
    return SNew(SBox)
    .MaxDesiredWidth(400)
    [
        HostWidget
    ];
}

TSharedPtr<SComboBox<TSharedPtr<FDungeonBuilderClassCombo::FBuilderClassEntry>>> FDungeonBuilderClassCombo::CreateComboWidget() {
    TWeakPtr<FDungeonBuilderClassCombo> WeakThisPtr = SharedThis(this);
    
    return SNew(SComboBox<TSharedPtr<FBuilderClassEntry>>)
        .OptionsSource(&BuilderClassComboItems)
        .OnComboBoxOpening(this, &FDungeonBuilderClassCombo::RegenerateBuilderComboItems)
        .OnGenerateWidget(this, &FDungeonBuilderClassCombo::GenerateBuilderClassComboRowItem)
        .OnSelectionChanged_Lambda([this](const TSharedPtr<FBuilderClassEntry>& InItem, ESelectInfo::Type InSelectType)
        {
            if (InItem.IsValid()) {
                SelectedBuilderItem = InItem;
            }
            
            // This is for the initial selection.
            if (InSelectType == ESelectInfo::Direct) {
                return;
            }

            SetBuilder(InItem->BuilderClass);
        })
        .Content()
        [
            SNew(STextBlock)
            .Text_Lambda([WeakThis = WeakThisPtr]() -> FText {
                TSharedPtr<FDungeonBuilderClassCombo> This = WeakThis.Pin();
                return This.IsValid() && This->SelectedBuilderItem.IsValid() && This->SelectedBuilderItem->BuilderClass
                    ? This->SelectedBuilderItem->BuilderClass->GetDisplayNameText()
                    : LOCTEXT("BuilderClass_DefaultNullText", "[None]");
            })
        ];
}

void FDungeonBuilderClassCombo::CreateBuilderCombo(FDetailWidgetRow& InRowWidget) {
    InRowWidget.NameContent()
    [
        SNew(STextBlock)
        .Text(LOCTEXT("BuilderClassText", "Builder Class"))
    ]
    .ValueContent()
    [
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot()
        .FillWidth(1.0)
        [
            CreateComboWidget().ToSharedRef()
        ]
        +SHorizontalBox::Slot()
        .AutoWidth()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(2.0f, 1.0f)
        [
            PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateSP(this, &FDungeonBuilderClassCombo::UseSelectedBuilderClass))
        ]
        +SHorizontalBox::Slot()
        .AutoWidth()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(2.0f, 1.0f)
        [
            PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FDungeonBuilderClassCombo::OnBrowseToSelectedBuilderClass))
        ]
    ];
}


///////////////////////////// FDungeonActorEditorCustomization /////////////////////////////
void FDungeonActorEditorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    IDetailCategoryBuilder& ActionsCategory = DetailBuilder.EditCategory("Dungeon Architect", FText::GetEmpty(), ECategoryPriority::Important);
    ActionsCategory.AddCustomRow(LOCTEXT("DungeonCommand_FilterBuildDungeon", "build dungeon"))
            .WholeRowContent()
    [
        SNew(SButton)
			.Text(LOCTEXT("DungeonCommand_BuildDungeon", "Build Dungeon"))
			.VAlign(VAlign_Center)
			.OnClicked(FOnClicked::CreateStatic(&FDungeonActorEditorCustomization::RebuildDungeon, &DetailBuilder))
    ];

    ActionsCategory.AddCustomRow(LOCTEXT("DungeonCommand_FilterDestroyDungeon", "destroy dungeon"))
            .WholeRowContent()
    [
        SNew(SButton)
			.Text(LOCTEXT("DungeonCommand_DestroyDungeon", "Destroy Dungeon"))
			.VAlign(VAlign_Center)
			.OnClicked(FOnClicked::CreateStatic(&FDungeonActorEditorCustomization::DestroyDungeon, &DetailBuilder))
    ];

    ActionsCategory.AddCustomRow(LOCTEXT("DungeonCommand_FilterRandomizeSeed", "randomize seed"))
            .WholeRowContent()
    [
        SNew(SButton)
			.Text(LOCTEXT("DungeonCommand_RandomizeSeed", "Randomize Seed"))
			.VAlign(VAlign_Center)
			.OnClicked(FOnClicked::CreateStatic(&FDungeonActorEditorCustomization::RandomizeSeed, &DetailBuilder))
    ];

    ActionsCategory.AddCustomRow(LOCTEXT("DungeonCommand_FilterHelpSection", "help support"))
            .WholeRowContent()
    [
        
        SNew(SBorder)
        .Padding(10)
        [
            SNew(SVerticalBox)
            +SVerticalBox::Slot()
            .Padding(0, 0, 0, 4)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("DACommand_Support", "Get Help / Support"))
            ]
            +SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SButton)
                        .Text(LOCTEXT("DungeonCommand_SupportForum", "Support Forum"))
                        .OnClicked(FOnClicked::CreateStatic(&FDungeonActorEditorCustomization::LaunchSupportForum, &DetailBuilder))
                ]
                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SButton)
                        .Text(LOCTEXT("DungeonCommand_Discord", "Discord"))
                        .OnClicked(FOnClicked::CreateStatic(&FDungeonActorEditorCustomization::LaunchDiscord, &DetailBuilder))
                ]
            ]
            +SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SButton)
                    .Text(LOCTEXT("DungeonCommand_Documentation", "Documentation"))
                    .OnClicked(FOnClicked::CreateStatic(&FDungeonActorEditorCustomization::LaunchDocumentation, &DetailBuilder))
            ]
        ]
    ];

    IDetailCategoryBuilder& DungeonCategory = DetailBuilder.EditCategory("Dungeon", FText::GetEmpty(), ECategoryPriority::Important);
    FDetailWidgetRow& BuilderClassWidgetRow = DungeonCategory.AddCustomRow(LOCTEXT("BuilderDropDown_Filter", "builder"));
    
    ADungeon* Dungeon = DACustomize::GetDungeon(&DetailBuilder);
    {
        const FDungeonBuilderClassCombo::FnGetBuilder GetBuilder = [Dungeon]() { return Dungeon ? Dungeon->BuilderClass : nullptr; };
        const FDungeonBuilderClassCombo::FnSetBuilder SetBuilder = [this, Dungeon](TSubclassOf<UDungeonBuilder> InBuilderClass) {
            Dungeon->SetBuilderClass(InBuilderClass);
            DACustomize::NotifyPropertyChanged(Dungeon, GET_MEMBER_NAME_CHECKED(ADungeon, BuilderClass));
        };
        BuilderClassCustomization = MakeShareable(new FDungeonBuilderClassCombo(GetBuilder, SetBuilder));
        BuilderClassCustomization->CreateBuilderCombo(BuilderClassWidgetRow);
    }
    
    // Hide the default Builder Class property as we have a custom builder drop down 
    {
        const TSharedRef<IPropertyHandle> BuilderClassDefaultPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ADungeon, BuilderClass));
        BuilderClassDefaultPropertyHandle->MarkHiddenByCustomization();
    }
    

    ////////////// Builder specific customization ///////////////
    // Show snap configuration customization
    if (Dungeon && Dungeon->GetBuilder()) {
        USnapMapDungeonBuilder* SnapMapBuilder = Cast<USnapMapDungeonBuilder>(Dungeon->GetBuilder());
        if (SnapMapBuilder) {

        }
    }


    ////////////// Config Category //////////////
    if (Dungeon) {
        UDungeonConfig* ConfigObject = Dungeon->GetConfig();
        if (ConfigObject) {
            DACustomize::ShowDungeonConfigProperties(DetailBuilder, ConfigObject);
        }
    }

    ////////////// Level Streaming Category //////////////
    if (Dungeon) {
        UDungeonBuilder* Builder = Dungeon->GetBuilder();
        if (Builder && Builder->SupportsLevelStreaming()) {
            DACustomize::ShowLevelStreamingProperties(DetailBuilder, &Dungeon->LevelStreamingConfig);
        }
    }

    // Hide unsupported properties
    if (Dungeon) {
        UDungeonBuilder* Builder = Dungeon->GetBuilder();
        if (Builder) {
            TArray<FName> PropertyNames;
            PropertyNames.Add(GET_MEMBER_NAME_CHECKED(ADungeon, Themes));
            PropertyNames.Add(GET_MEMBER_NAME_CHECKED(ADungeon, MarkerEmitters));
            PropertyNames.Add(GET_MEMBER_NAME_CHECKED(ADungeon, EventListeners));
            for (const FName& PropertyName : PropertyNames) {
                const TSharedRef<IPropertyHandle> PropertyHandle = DetailBuilder.GetProperty(PropertyName);
                if (!Builder->SupportsProperty(PropertyName)) {
                    PropertyHandle->MarkHiddenByCustomization();
                }
            }
        }
    }

    // Show a experimental builder warning message
    if (Dungeon) {
        bool bIsExperimental = false;
        bool bIsEarlyAccess = false;
        UDungeonBuilder* Builder = Dungeon->GetBuilder();
        FString MostDerivedClassName;

        if (Builder) {
            FObjectEditorUtils::GetClassDevelopmentStatus(Builder->GetClass(), bIsExperimental, bIsEarlyAccess,
                                                          MostDerivedClassName);
        }

        const FText BuilderClassName = Builder ? Builder->GetClass()->GetDisplayNameText() : LOCTEXT("DefaultBuilderName", "Builder");

        if (bIsExperimental || bIsEarlyAccess) {
            const FText WarningText = bIsExperimental
                                          ? FText::Format(
                                              LOCTEXT("ExperimentalBuilderWarning",
                                                      "Uses experimental builder: {0}.  This builder is still a work in progress"),
                                              BuilderClassName)
                                          : FText::Format(
                                              LOCTEXT("EarlyAccessClassWarning",
                                                      "Uses early access builder: {0}.  This builder is still a work in progress"),
                                              BuilderClassName);
            const FText SearchString = WarningText;
            const FText Tooltip = FText::Format(
                LOCTEXT("ExperimentalClassTooltip", "{0} is still a work in progress and is not fully supported"),
                BuilderClassName);
            const FString ExcerptName = bIsExperimental
                                            ? TEXT("ActorUsesExperimentalClass")
                                            : TEXT("ActorUsesEarlyAccessClass");
            const FSlateBrush* WarningIcon = FAppStyle::GetBrush(
                bIsExperimental ? "PropertyEditor.ExperimentalClass" : "PropertyEditor.EarlyAccessClass");
            const FLinearColor BackgroundColor = bIsExperimental
                ? FLinearColor(1, .4, 0, .25)
                : FLinearColor(.4, .4, 0, .25);

            FDetailWidgetRow& WarningRow = DungeonCategory
            .AddCustomRow(SearchString)
            .WholeRowContent()
            [
                SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("SettingsEditor.CheckoutWarningBorder"))
				.BorderBackgroundColor(BackgroundColor)
                [
                    SNew(SHorizontalBox)
					.ToolTip(IDocumentation::Get()->CreateToolTip(Tooltip, nullptr, TEXT("Shared/LevelEditor"), ExcerptName))
					.Visibility(EVisibility::Visible)

                    +SHorizontalBox::Slot()
                    .VAlign(VAlign_Center)
                    .AutoWidth()
                    .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SImage)
                        .Image(WarningIcon)
                    ]

                    +SHorizontalBox::Slot()
                    .VAlign(VAlign_Center)
                    .AutoWidth()
                    .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
					    .Text(WarningText)
				        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
            ];
        }
    }
}

TSharedRef<IDetailCustomization> FDungeonActorEditorCustomization::MakeInstance() {
    return MakeShareable(new FDungeonActorEditorCustomization);
}

FReply FDungeonActorEditorCustomization::RebuildDungeon(IDetailLayoutBuilder* DetailBuilder) {
    ADungeon* Dungeon = DACustomize::GetDungeon(DetailBuilder);

    if (!Dungeon) {
        // invalid state
        return FReply::Handled();
    }

    UDungeonBuilder* Builder = Dungeon->GetBuilder();
    if (!Builder) {
        FDungeonEditorUtils::ShowNotification(NSLOCTEXT("DungeonMissingBuilder", "BuilderNotAssigned", "Dungeon Builder not assigned"));
        return FReply::Handled();
    }

    // Check if we can build the dungeon
    {
        FString ErrorMessage;
        if (!Builder->CanBuildDungeon(ErrorMessage)) {
            FDungeonEditorUtils::ShowNotification(FText::FromString(ErrorMessage));
            return FReply::Handled();
        }
    }


    bool bContainValidThemes = false;
    if (Builder->SupportsTheming()) {
        // make sure we have at least one theme defined
        for (UDungeonThemeAsset* Theme : Dungeon->Themes) {
            if (Theme) {
                bContainValidThemes = true;
                break;
            }
        }
    }
    else {
        // themes are not supported. We don't require a theme to be defined
        bContainValidThemes = true;
    }

    if (!bContainValidThemes) {
        // Notify user that a theme has not been assigned
        FDungeonEditorUtils::ShowNotification(NSLOCTEXT("DungeonMissingTheme", "ThemeNotAssigned", "Dungeon Theme Not Assigned"));
        return FReply::Handled();
    }

    FDungeonEditorUtils::BuildDungeon(Dungeon);

    return FReply::Handled();
}

FReply FDungeonActorEditorCustomization::DestroyDungeon(IDetailLayoutBuilder* DetailBuilder) {
    if (ADungeon* Dungeon = DACustomize::GetDungeon(DetailBuilder)) {
        FDungeonEditorUtils::SwitchToRealtimeMode();
        Dungeon->DestroyDungeon();
    }

    return FReply::Handled();
}


FReply FDungeonActorEditorCustomization::RandomizeSeed(IDetailLayoutBuilder* DetailBuilder) {
    if (const ADungeon* Dungeon = DACustomize::GetDungeon(DetailBuilder)) {
        Dungeon->GetConfig()->Seed = FMath::Rand();
    }

    return FReply::Handled();
}

FReply FDungeonActorEditorCustomization::OpenHelpSystem(IDetailLayoutBuilder* DetailBuilder) {
    FDungeonArchitectHelpSystem::LaunchHelpSystemTab();
    return FReply::Handled();
}

FReply FDungeonActorEditorCustomization::LaunchSupportForum(IDetailLayoutBuilder* DetailBuilder) {
    FPlatformProcess::LaunchURL(FDungeonEditorConstants::URL_FORUMS, nullptr, nullptr);
    return FReply::Handled();
}

FReply FDungeonActorEditorCustomization::LaunchDiscord(IDetailLayoutBuilder* DetailBuilder) {
    FPlatformProcess::LaunchURL(FDungeonEditorConstants::URL_DISCORD, nullptr, nullptr);
    return FReply::Handled();
}

FReply FDungeonActorEditorCustomization::LaunchDocumentation(IDetailLayoutBuilder* DetailBuilder) {
    FPlatformProcess::LaunchURL(FDungeonEditorConstants::URL_DOCS, nullptr, nullptr);
    return FReply::Handled();
}

void FDungeonArchitectMeshNodeCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Dungeon");

    Category.AddCustomRow(LOCTEXT("DungeonCommand_FilterBuildDungeon", "build dungeon"))
    .WholeRowContent()
    [
        SNew(SButton)
			.Text(LOCTEXT("DungeonCommand_ShowMeshNodeProperties", "Edit Advanced Properties"))
			.OnClicked(FOnClicked::CreateStatic(&FDungeonArchitectMeshNodeCustomization::EditAdvancedOptions, &DetailBuilder))
    ];
}

FReply FDungeonArchitectMeshNodeCustomization::EditAdvancedOptions(IDetailLayoutBuilder* DetailBuilder) {
    TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
    DetailBuilder->GetObjectsBeingCustomized(ObjectsBeingCustomized);

    if (ObjectsBeingCustomized.Num() > 1) {
        FDungeonEditorUtils::ShowNotification(NSLOCTEXT("TooManyObjects", "TooManyObjects", "Only a single node can be edited at a time"));
        return FReply::Handled();
    }

    UObject* ObjectToEdit = nullptr;
    if (ObjectsBeingCustomized.Num() == 0) {
        ObjectToEdit = ObjectsBeingCustomized[0].Get();
    }

    if (!ObjectToEdit) {
        FDungeonEditorUtils::ShowNotification(NSLOCTEXT("ObjectNotValid", "ObjectNotValid", "Node state is not valid"));
        return FReply::Handled();
    }

    if (UEdGraphNode_DungeonMesh* MeshNode = Cast<UEdGraphNode_DungeonMesh>(ObjectToEdit)) {
        FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        TMap<UObject*, UObject*> OldToNewObjectMap;
    }

    return FReply::Handled();
}

void FDungeonEditorViewportPropertiesCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    UDungeonEditorViewportProperties* ViewportProperties = FDungeonEditorUtils::GetBuilderObject<UDungeonEditorViewportProperties>(&DetailBuilder);
    if (!ViewportProperties) {
        return;
    }
    
    
    // Hide the builder config, we have a custom drop down for it
    {
        const TSharedRef<IPropertyHandle> BuilderClassPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UDungeonEditorViewportProperties, BuilderClass));
        BuilderClassPropertyHandle->MarkHiddenByCustomization();
    }

    // Add a custom builder class drop down
    {
        IDetailCategoryBuilder& DungeonCategory = DetailBuilder.EditCategory("Builder", FText::GetEmpty(), ECategoryPriority::Important);
        FDetailWidgetRow& BuilderClassWidgetRow = DungeonCategory.AddCustomRow(LOCTEXT("BuilderDropDown_Filter", "builder"));
        const FDungeonBuilderClassCombo::FnGetBuilder GetBuilder = [ViewportProperties]() { return ViewportProperties ? ViewportProperties->BuilderClass : nullptr; };
        const FDungeonBuilderClassCombo::FnSetBuilder SetBuilder = [this, ViewportProperties](TSubclassOf<UDungeonBuilder> InBuilderClass) {
            ViewportProperties->BuilderClass = InBuilderClass;
            DACustomize::NotifyPropertyChanged(ViewportProperties, GET_MEMBER_NAME_CHECKED(UDungeonEditorViewportProperties, BuilderClass));
        };
        
        BuilderClassCustomization = MakeShareable(new FDungeonBuilderClassCombo(GetBuilder, SetBuilder));
        BuilderClassCustomization->CreateBuilderCombo(BuilderClassWidgetRow);
    }

    // Show the config category
    DACustomize::ShowDungeonConfigProperties(DetailBuilder, ViewportProperties->DungeonConfig);
}

TSharedRef<IDetailCustomization> FDungeonEditorViewportPropertiesCustomization::MakeInstance() {
    return MakeShareable(new FDungeonEditorViewportPropertiesCustomization);
}

/////////////////////////////// Volume Customization //////////////////////////
void FDungeonArchitectVolumeCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Dungeon");
    Category.AddCustomRow(LOCTEXT("DungeonVolumeCommand_FilterBuildDungeon", "build dungeon"))
    .ValueContent()
    [
        SNew(SButton)
			.Text(LOCTEXT("DungeonVolumeCommand_BuildDungeon", "Rebuild Dungeon"))
			.OnClicked(FOnClicked::CreateStatic(&FDungeonArchitectVolumeCustomization::RebuildDungeon, &DetailBuilder))
    ];
}


TSharedRef<IDetailCustomization> FDungeonArchitectVolumeCustomization::MakeInstance() {
    return MakeShareable(new FDungeonArchitectVolumeCustomization);
}

FReply FDungeonArchitectVolumeCustomization::RebuildDungeon(IDetailLayoutBuilder* DetailBuilder) {
    ADungeonVolume* Volume = FDungeonEditorUtils::GetBuilderObject<ADungeonVolume>(DetailBuilder);
    if (Volume && Volume->Dungeon) {
        FDungeonEditorUtils::BuildDungeon(Volume->Dungeon);
    }
    return FReply::Handled();
}

void FDungeonPropertyChangeListener::Initialize() {
    FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(SharedThis(this), &FDungeonPropertyChangeListener::OnPropertyChanged);
}

void FDungeonPropertyChangeListener::OnPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event) {
    if (Object->IsA(ADungeon::StaticClass())) {
        const FName PropertyName = (Event.Property != nullptr) ? Event.Property->GetFName() : NAME_None;
        if (PropertyName == GET_MEMBER_NAME_CHECKED(ADungeon, BuilderClass)) {
            // Refresh the properties window
            UE_LOG(LogDungeonCustomization, Log, TEXT("Dungeon builder class changed"));
            FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
            TArray<UObject*> ObjectList;
            if (Object) {
                ObjectList.Add(Object);
            }
            PropertyEditorModule.UpdatePropertyViews(ObjectList);
        }
    }
    else if (Object->IsA(ASnapModuleBoundShape::StaticClass())) {
        const FName PropertyName = (Event.Property != nullptr) ? Event.Property->GetFName() : NAME_None;
        if (PropertyName == GET_MEMBER_NAME_CHECKED(ASnapModuleBoundShape, BoundsType)) {
            // Refresh the properties window
            UE_LOG(LogDungeonCustomization, Log, TEXT("SnapMap Bounds Type changed"));
            FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
            TArray<UObject*> ObjectList;
            if (Object) {
                ObjectList.Add(Object);
            }
            PropertyEditorModule.UpdatePropertyViews(ObjectList);
        }
    }
}

//////////////// FDungeonDebugCustomization ////////////////////
void FDungeonDebugCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Debug Commands");
    for (int i = 0; i < 10; i++) {
        FText Caption = FText::FromString("Command " + FString::FromInt(i));

        Category.AddCustomRow(Caption)
        .ValueContent()
        [
            SNew(SButton)
				.Text(Caption)
				.OnClicked(FOnClicked::CreateStatic(&FDungeonDebugCustomization::ExecuteCommand, &DetailBuilder, i))
        ];
    }
}

TSharedRef<IDetailCustomization> FDungeonDebugCustomization::MakeInstance() {
    return MakeShareable(new FDungeonDebugCustomization);
}

FReply FDungeonDebugCustomization::ExecuteCommand(IDetailLayoutBuilder* DetailBuilder, int32 CommandID) {
    ADungeonDebug* Debug = FDungeonEditorUtils::GetBuilderObject<ADungeonDebug>(DetailBuilder);
    if (Debug) {
        Debug->ExecuteDebugCommand(CommandID);
    }

    if (CommandID == 8) {
        IContentBrowserSingleton& ContentBrowserSingleton = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
        TArray<FString> SelectedPaths;
        ContentBrowserSingleton.GetSelectedPathViewFolders(SelectedPaths);
        for (const FString& SelectedPath : SelectedPaths) {
            UE_LOG(LogDungeonCustomization, Log, TEXT("Selected Path: %s"), *SelectedPath);
        }
    }

    return FReply::Handled();
}

//////////////// FSnapModuleDatabaseCustomization ////////////////////
void FSnapModuleDatabaseCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    IDetailCategoryBuilder& SnapMapCategory = DetailBuilder.EditCategory("Module Cache");

    SnapMapCategory.AddCustomRow(LOCTEXT("SnapMapCommand_FilterRebuildModuleCache", "Build Module Cache"))
    .WholeRowContent()
    [
        SNew(SBorder)
			.BorderImage(FDungeonArchitectStyle::Get().GetBrush("DungeonArchitect.RoundDarkBorder"))
			.BorderBackgroundColor(FColor(32, 32, 32))
			.Padding(FMargin(10, 10))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("SnapMap_RebuildCacheHelpText",
                                  "Rebuild the cache whenever you change the module list below or if the module level files have been changed in any way.\nThis needs to be done in the editor before building the dungeon at runtime to speed things up\n"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SButton)
					.Text(LOCTEXT("SnapMapCommand_RebuildModuleCache", "Build Module Cache"))
					.OnClicked(FOnClicked::CreateStatic(&FSnapModuleDatabaseCustomization::BuildDatabaseCache,
                                                        &DetailBuilder))
            ]
        ]
    ];
}

TSharedRef<IDetailCustomization> FSnapModuleDatabaseCustomization::MakeInstance() {
    return MakeShareable(new FSnapModuleDatabaseCustomization);
}

FReply FSnapModuleDatabaseCustomization::BuildDatabaseCache(IDetailLayoutBuilder* DetailBuilder) {
    USnapMapModuleDatabase* ModuleDatabase = FDungeonEditorUtils::GetBuilderObject<USnapMapModuleDatabase>(DetailBuilder);
    if (ModuleDatabase) {
        FSnapMapModuleDBUtils::BuildModuleDatabaseCache(ModuleDatabase);
    }

    return FReply::Handled();
}

//////////////// FSnapGridFlowModuleDatabaseCustomization ////////////////////
void FSnapGridFlowModuleDatabaseCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    IDetailCategoryBuilder& ModuleCacheCategory = DetailBuilder.EditCategory("Module Cache", FText::GetEmpty(), ECategoryPriority::Variable);

    ModuleCacheCategory.AddCustomRow(LOCTEXT("SnapGridFlowCommand_FilterRebuildModuleCache", "Build Module Cache"))
    .WholeRowContent()
    [
        SNew(SBorder)
            .BorderImage(FDungeonArchitectStyle::Get().GetBrush("DungeonArchitect.RoundDarkBorder"))
            .BorderBackgroundColor(FColor(32, 32, 32))
            .Padding(FMargin(10, 10))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .AutoWrapText(true)
                    .Text(LOCTEXT("SnapGridFlow_RebuildCacheHelpText",
                                  "Rebuild the cache whenever you change the module list below or if the module level files have been changed in any way.\nThis needs to be done in the editor before building the dungeon at runtime to speed things up\n"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SButton)
                    .Text(LOCTEXT("SnapGridFlowCommand_RebuildModuleCache", "Build Module Cache"))
                    .OnClicked(FOnClicked::CreateStatic(&FSnapGridFlowModuleDatabaseCustomization::BuildDatabaseCache,
                                                        &DetailBuilder))
            ]
        ]
    ];

    IDetailCategoryBuilder& ToolsCategory = DetailBuilder.EditCategory("Tools", FText::GetEmpty(), ECategoryPriority::Important);
    ToolsCategory.AddCustomRow(LOCTEXT("SnapGridFlowCommand_FilterToolsHeader", "Tools"))
            .WholeRowContent()
    [
        SNew(SBorder)
            .BorderImage(FDungeonArchitectStyle::Get().GetBrush("DungeonArchitect.RoundDarkBorder"))
            .BorderBackgroundColor(FColor(32, 32, 32))
            .Padding(FMargin(10, 10))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SButton)
                    .Text(LOCTEXT("SnapGridFlowCommand_ToolAddModulesFromDir", "Add All Modules from Directory"))
                    .OnClicked(FOnClicked::CreateRaw(this, &FSnapGridFlowModuleDatabaseCustomization::HandleTool_AddModulesFromDir, &DetailBuilder))
            ]
        ]
    ];
}

void FSnapGridFlowModuleDatabaseCustomization::AddReferencedObjects(FReferenceCollector& Collector) {
    if (DirImportSettings) {
        Collector.AddReferencedObject(DirImportSettings);
    }
}

FString FSnapGridFlowModuleDatabaseCustomization::GetReferencerName() const {
    static const FString NameString = TEXT("FSnapGridFlowModuleDatabaseCustomization");
    return NameString;
}

TSharedRef<IDetailCustomization> FSnapGridFlowModuleDatabaseCustomization::MakeInstance() {
    return MakeShareable(new FSnapGridFlowModuleDatabaseCustomization);
}

FReply FSnapGridFlowModuleDatabaseCustomization::BuildDatabaseCache(IDetailLayoutBuilder* DetailBuilder) {
    USnapGridFlowModuleDatabase* ModuleDatabase = FDungeonEditorUtils::GetBuilderObject<USnapGridFlowModuleDatabase>(DetailBuilder);
    if (ModuleDatabase) {
        FSnapGridFlowEditorUtils::BuildModuleDatabaseCache(ModuleDatabase);
    }

    return FReply::Handled();
}

FReply FSnapGridFlowModuleDatabaseCustomization::HandleTool_AddModulesFromDir(IDetailLayoutBuilder* DetailBuilder) {
    TObjectPtr<USnapGridFlowModuleDatabase> ModuleDatabase = FDungeonEditorUtils::GetBuilderObject<USnapGridFlowModuleDatabase>(DetailBuilder);
    if (ModuleDatabase) {
        
        static FString Path = "/Game/";
        TSharedRef<SDlgPickPath> SelectPathDlg =
            SNew(SDlgPickPath)
            .Title(LOCTEXT("ChooseImportRootContentPath", "Select the directory that contains the modules"))
            .DefaultPath(FText::FromString(Path));
        
        if (SelectPathDlg->ShowModal() != EAppReturnType::Cancel) {
            const FString ImportPath = SelectPathDlg->GetPath().ToString();
            // Ask the user if we want to rotate these modules while stitching
            {
                if (!DirImportSettings) {
                    DirImportSettings = NewObject<USnapGridFlowModuleDBImportSettings>();
                }
                
                FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
                FDetailsViewArgs DetailsViewArgs;
                DetailsViewArgs.bUpdatesFromSelection = false;
                DetailsViewArgs.bLockable = false;
                DetailsViewArgs.bAllowSearch = true;
                DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
                DetailsViewArgs.bHideSelectionTip = true;
                
                TSharedRef<IDetailsView> PropertyEditor = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
                PropertyEditor->SetObject(DirImportSettings);
                
                TSharedRef<SWindow> Window = SNew(SWindow)
                    .Title(NSLOCTEXT("SGFModuleDB", "DirImportTitle", "Register modules from directory"))
                    .AutoCenter(EAutoCenter::PreferredWorkArea)
                    .SizingRule(ESizingRule::UserSized)
                    .ClientSize(FVector2D(350, 350))
                    .MinWidth(300)
                    .MinHeight(300);


                const float Padding = 5.0f;
                const TSharedPtr<SWidget> Host =
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot()
                    .Padding(Padding)
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT("SGFModuleDB", "DirImportDesc", "Module Settings"))
                    ]
                    +SVerticalBox::Slot()
                    .Padding(Padding)
                    .FillHeight(1.0f)
                    [
                        PropertyEditor
                    ]
                    +SVerticalBox::Slot()
                    .Padding(Padding)
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            SNullWidget::NullWidget
                        ]
                        +SHorizontalBox::Slot()
                        .Padding(Padding)
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .Text(NSLOCTEXT("SGFModuleDB", "ButtonOK", "OK"))
                            .OnClicked_Raw(this, &FSnapGridFlowModuleDatabaseCustomization::HandleTool_AddModulesFromDir_ButtonOK, DirImportSettings, ModuleDatabase, ImportPath)
                        ]
                        +SHorizontalBox::Slot()
                        .Padding(Padding)
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .Text(NSLOCTEXT("SGFModuleDB", "ButtonCancel", "Cancel"))
                            .OnClicked_Raw(this, &FSnapGridFlowModuleDatabaseCustomization::HandleTool_AddModulesFromDir_ButtonCancel)
                        ]
                    ];


                Window->SetContent(Host.ToSharedRef());
                
                TSharedPtr<SWindow> ParentWindow;
                if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
                {
                    IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
                    ParentWindow = MainFrame.GetParentWindow();
                }

                DirImportSettingsWindow = Window;
                FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
            }
        }
    }
    
    return FReply::Handled();
}

FReply FSnapGridFlowModuleDatabaseCustomization::HandleTool_AddModulesFromDir_ButtonOK(TObjectPtr<USnapGridFlowModuleDBImportSettings> InSettings, TObjectPtr<USnapGridFlowModuleDatabase> ModuleDatabase, FString ImportPath) {
    if (InSettings && ModuleDatabase) {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
        TArray<FAssetData> AssetDataList;
        AssetRegistry.GetAssetsByPath(*ImportPath, AssetDataList, true, true);
        //static const FName MapClassName = UWorld::StaticClass()->GetFName();
        bool bModified = false;
        for (const FAssetData& AssetData : AssetDataList) {
            if (AssetData.AssetClassPath == UWorld::StaticClass()->GetClassPathName()) {
                FSnapGridFlowModuleDatabaseItem& ModuleInfo = ModuleDatabase->Modules.AddDefaulted_GetRef();
                ModuleInfo.Level = AssetData.GetAsset();
                ModuleInfo.Category = InSettings->Category;
                ModuleInfo.bAllowRotation = InSettings->bAllowRotation;
                    
                bModified = true;
            } 
        }

        if (bModified) {
            ModuleDatabase->Modify();
        }
    }
    
    // Close the settings window
    if (DirImportSettingsWindow.IsValid()) {
        DirImportSettingsWindow.Pin()->RequestDestroyWindow();
        DirImportSettingsWindow.Reset();
    }
    return FReply::Handled();
}

FReply FSnapGridFlowModuleDatabaseCustomization::HandleTool_AddModulesFromDir_ButtonCancel() {
    if (DirImportSettingsWindow.IsValid()) {
        DirImportSettingsWindow.Pin()->RequestDestroyWindow();
        DirImportSettingsWindow.Reset();
    }
    return FReply::Handled();
}

///////////////////////////////////// FDAExecRuleNodeCustomization /////////////////////////////////////

void FDAExecRuleNodeCustomization::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) {
    CachedDetailBuilder = DetailBuilder;
    CustomizeDetails(*DetailBuilder);
}

void FDAExecRuleNodeCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    UEdGraphNode_GrammarExecRuleNode* Node = FDungeonEditorUtils::GetBuilderObject<UEdGraphNode_GrammarExecRuleNode>(&DetailBuilder);
    if (!Node) {
        return;
    }

    TSharedPtr<IPropertyHandle> ExecutionModeProperty = DetailBuilder.GetProperty(
        GET_MEMBER_NAME_CHECKED(UEdGraphNode_GrammarExecRuleNode, ExecutionMode), UEdGraphNode_GrammarExecRuleNode::StaticClass());
    ExecutionModeProperty->SetOnPropertyValueChanged(
        FSimpleDelegate::CreateSP(this, &FDAExecRuleNodeCustomization::OnExecutionModeChanged, Node));

    TSharedRef<FStructOnScope> ConfigRef = MakeShared<FStructOnScope>(FRuleNodeExecutionModeConfig::StaticStruct(),
                                                                      (uint8*)&Node->ExecutionConfig);

    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Execution Config",
                                                                  LOCTEXT("GrammarCategoryLabel", "Execution Config"),
                                                                  ECategoryPriority::Uncommon);
    ERuleNodeExecutionMode ExecutionMode = Node->ExecutionMode;
    if (ExecutionMode == ERuleNodeExecutionMode::RunWithProbability) {
        Category.AddExternalStructureProperty(
            ConfigRef, GET_MEMBER_NAME_CHECKED(FRuleNodeExecutionModeConfig, RunProbability),
            EPropertyLocation::Common);
    }
    else if (ExecutionMode == ERuleNodeExecutionMode::Iterate) {
        Category.AddExternalStructureProperty(
            ConfigRef, GET_MEMBER_NAME_CHECKED(FRuleNodeExecutionModeConfig, IterationCount),
            EPropertyLocation::Common);
    }
    else if (ExecutionMode == ERuleNodeExecutionMode::IterateRange) {
        Category.AddExternalStructureProperty(
            ConfigRef, GET_MEMBER_NAME_CHECKED(FRuleNodeExecutionModeConfig, IterationCountMin),
            EPropertyLocation::Common);
        Category.AddExternalStructureProperty(
            ConfigRef, GET_MEMBER_NAME_CHECKED(FRuleNodeExecutionModeConfig, IterationCountMax),
            EPropertyLocation::Common);
    }
}

void FDAExecRuleNodeCustomization::OnExecutionModeChanged(UEdGraphNode_GrammarExecRuleNode* Node) {
    IDetailLayoutBuilder* DetailBuilder = CachedDetailBuilder.Pin().Get();
    if (DetailBuilder) {
        DetailBuilder->ForceRefreshDetails();
    }
}

TSharedRef<IDetailCustomization> FDAExecRuleNodeCustomization::MakeInstance() {
    return MakeShareable(new FDAExecRuleNodeCustomization);
}


///////////////////////////////////// FUMarkerGenPatternRuleCustomization /////////////////////////////////////
void FMGPatternRuleCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Pattern Rule");
    Category.AddCustomRow(LOCTEXT("MGPatternRule_ColorRow", "RandomizeColor"))
    .WholeRowContent()
    [
        SNew(SButton)
            .Text(LOCTEXT("RandomizeColorLabel", "Randomize Color"))
            .OnClicked(FOnClicked::CreateStatic(&FMGPatternRuleCustomization::RandomizeColor, &DetailBuilder))
    ];
}

FReply FMGPatternRuleCustomization::RandomizeColor(IDetailLayoutBuilder* DetailBuilder) {
    UMarkerGenPatternRule* PatternRule = FDungeonEditorUtils::GetBuilderObject<UMarkerGenPatternRule>(DetailBuilder);
    if (PatternRule) {
        PatternRule->AssignRandomColor();
        PatternRule->Modify();

        const TSharedRef<IPropertyHandle> ColorPropertyHandle = DetailBuilder->GetProperty(GET_MEMBER_NAME_CHECKED(UMarkerGenPatternRule, Color));
        ColorPropertyHandle->NotifyPostChange(EPropertyChangeType::Unspecified);
        ColorPropertyHandle->NotifyFinishedChangingProperties();
    }
    return FReply::Handled();
}

TSharedRef<IDetailCustomization> FMGPatternRuleCustomization::MakeInstance() {
    return MakeShareable(new FMGPatternRuleCustomization);
}

void FMGPatternGridLayerCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    DetailBuilder.EditCategory("Marker Generator").SetSortOrder(0);
    DetailBuilder.EditCategory("Pattern Matching").SetSortOrder(1);
    DetailBuilder.EditCategory("Constraints").SetSortOrder(2);
    DetailBuilder.EditCategory("Advanced").SetSortOrder(3);
}

TSharedRef<IDetailCustomization> FMGPatternGridLayerCustomization::MakeInstance() {
    return MakeShareable(new FMGPatternGridLayerCustomization);
}

///////////////////////////////////// FDungeonCanvasMaterialLayerCustomization /////////////////////////////////////


class FDungeonCanvasColorPicker {
public:
    static FDungeonCanvasColorPicker& Get() { return Singleton; }

    struct FSettings {
        FLinearColor InitialColor;
        FMaterialParameterInfo ParamInfo;
        TWeakObjectPtr<UMaterialInstanceConstant> MaterialInstanceWeakPtr;
        TSharedPtr<SWidget> ParentDetailsWidget;
        TWeakObjectPtr<UDungeonCanvasMaterialLayer> CanvasMatLayerPtr;
        TWeakObjectPtr<UDungeonCanvasBlueprint> CanvasBlueprintPtr;
    };

    void ShowColorPicker(const FSettings& InSettings) {
        Settings = InSettings;
        
        GEditor->BeginTransaction(FText::Format(LOCTEXT("SetColorProperty", "Edit Color: {0}"), FText::FromName(Settings.ParamInfo.Name)));
        
        FColorPickerArgs PickerArgs;
        PickerArgs.bUseAlpha = true;
        PickerArgs.bOnlyRefreshOnMouseUp = false;
        PickerArgs.bOnlyRefreshOnOk = false;
        PickerArgs.sRGBOverride = false;
        PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
        PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateRaw(this, &FDungeonCanvasColorPicker::OnSetColorFromColorPicker);
        PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateRaw(this, &FDungeonCanvasColorPicker::OnColorPickerCancelled);
        PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateRaw(this, &FDungeonCanvasColorPicker::OnColorPickerWindowClosed);
        PickerArgs.InitialColor = Settings.InitialColor;
        PickerArgs.ParentWidget = Settings.ParentDetailsWidget;
        PickerArgs.OptionalOwningDetailsView = Settings.ParentDetailsWidget;
        FWidgetPath ParentWidgetPath;
        if (FSlateApplication::Get().FindPathToWidget(Settings.ParentDetailsWidget.ToSharedRef(), ParentWidgetPath))
        {
            PickerArgs.bOpenAsMenu = FSlateApplication::Get().FindMenuInWidgetPath(ParentWidgetPath).IsValid();
        }
        
	    OpenColorPicker(PickerArgs);
    }
private:
    void OnSetColorFromColorPicker(FLinearColor NewColor) {
        SetMaterialColor(NewColor);
    }

    void OnColorPickerCancelled(FLinearColor OriginalColor) {
        SetMaterialColor(OriginalColor);
	    GEditor->CancelTransaction(0);
    }

    void OnColorPickerWindowClosed(const TSharedRef<SWindow>& Window) {
        GEditor->EndTransaction();
    }
    
    void SetMaterialColor(const FLinearColor& InColor) const {
        if (UMaterialInstanceConstant* Mat = Settings.MaterialInstanceWeakPtr.Get()) {
            Mat->SetVectorParameterValueEditorOnly(Settings.ParamInfo, InColor);
            UMaterialEditingLibrary::UpdateMaterialInstance(Mat);

            if (Settings.CanvasMatLayerPtr.IsValid()) {
                Settings.CanvasMatLayerPtr->Modify();
            }
            if (Settings.CanvasBlueprintPtr.IsValid()) {
                Settings.CanvasBlueprintPtr->Status = BS_Dirty;
            }
        }
    }
    
private:
    FSettings Settings;
    static FDungeonCanvasColorPicker Singleton;
};
FDungeonCanvasColorPicker FDungeonCanvasColorPicker::Singleton;

void FDungeonCanvasMaterialLayerCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
    UDungeonCanvasMaterialLayer* CanvasMatLayer = FDungeonEditorUtils::GetBuilderObject<UDungeonCanvasMaterialLayer>(&DetailBuilder);
    if (!CanvasMatLayer || !CanvasMatLayer->MaterialLayer) {
        return;
    }

    UDungeonCanvasBlueprint* CanvasBP = Cast<UDungeonCanvasBlueprint>(CanvasMatLayer->GetOuter());
    UMaterialInstanceConstant* MaterialInstance = CanvasBP ? CanvasBP->MaterialInstance : nullptr;
    if (!MaterialInstance) {
        return;
    }

    TArray<FGuid> TempGuids;
    TWeakObjectPtr<UMaterialInstanceConstant> MaterialInstanceWeakPtr = MaterialInstance;

    auto MarkDirty = [CanvasMatLayer, CanvasBP]() {
        CanvasMatLayer->Modify();
        CanvasBP->Status = BS_Dirty;
    };

    auto MakeResetToDefaultWidget = [](const TFunction<bool()>& FnIsResetToDefaultVisible, const TFunction<void()>& FnHandleResetToDefaultClicked) {
        static FSlateIcon EnabledResetToDefaultIcon(FAppStyle::Get().GetStyleSetName(), "PropertyWindow.DiffersFromDefault");
        static FSlateIcon DisabledResetToDefaultIcon(FAppStyle::Get().GetStyleSetName(), "NoBrush");
        FSlimHorizontalToolBarBuilder ToolbarBuilder(TSharedPtr<FUICommandList>(), FMultiBoxCustomization::None);
        ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
        ToolbarBuilder.SetStyle(&FAppStyle::Get(), "DetailsView.ExtensionToolBar");
        ToolbarBuilder.SetIsFocusable(false);
        ToolbarBuilder.AddToolBarButton(
            FUIAction(
                FExecuteAction::CreateLambda([FnHandleResetToDefaultClicked]() {
                    FnHandleResetToDefaultClicked();
                }),
                FCanExecuteAction::CreateLambda([FnIsResetToDefaultVisible]()
                {
                    return FnIsResetToDefaultVisible();
                })
            ),
            NAME_None,
            NSLOCTEXT("PropertyEditor", "ResetToDefault", "Reset to Default"),
            TAttribute<FText>::Create([FnIsResetToDefaultVisible]() {
                return FnIsResetToDefaultVisible() ?
                    NSLOCTEXT("PropertyEditor", "ResetToDefaultPropertyValueToolTip", "Reset this property to its default value.") :
                    FText::GetEmpty();
            }),
            TAttribute<FSlateIcon>::Create([FnIsResetToDefaultVisible]()
            {
                return FnIsResetToDefaultVisible() ?
                    EnabledResetToDefaultIcon :
                    DisabledResetToDefaultIcon;
            }));
        return ToolbarBuilder.MakeWidget();
    };

    struct FPropertyEntry {
        FName ParamName;
        FName Category = NAME_None;
        int32 SortPriority{};
        TSharedPtr<SWidget> Content;
    };
    TArray<FPropertyEntry> PropertyEntries;
    
    // Scalar Parameters
    {
        TArray<FMaterialParameterInfo> ScalarMaterialParams;
        MaterialInstance->GetAllScalarParameterInfo(ScalarMaterialParams, TempGuids);
        for (const FMaterialParameterInfo& ParamInfo : ScalarMaterialParams) {
            if (ParamInfo.Association == LayerParameter && ParamInfo.Index == CanvasMatLayer->LayerIndex) {
                float SliderMin{}, SliderMax{};
                MaterialInstance->GetScalarParameterSliderMinMax(ParamInfo, SliderMin, SliderMax);
                TSharedPtr<SNumericEntryBox<float>> EntryBox;
                auto FnGetValue = [ParamInfo, MaterialInstanceWeakPtr]() {
                    if (const UMaterialInstanceConstant* Mat = MaterialInstanceWeakPtr.Get()) {
                        float Value{};
                        Mat->GetScalarParameterValue(ParamInfo, Value);
                        return TOptional<float>(Value);
                    }
                    else {
                        return TOptional<float>(0);
                    }
                };

                auto FnSetValue = [ParamInfo, MaterialInstanceWeakPtr, MarkDirty](float NewValue) {
                    if (UMaterialInstanceConstant* Mat = MaterialInstanceWeakPtr.Get()) {
                        Mat->SetScalarParameterValueEditorOnly(ParamInfo, NewValue);
                        UMaterialEditingLibrary::UpdateMaterialInstance(Mat);
                        MarkDirty();
                    }
                };

                if (FMath::IsNearlyEqual(SliderMin, SliderMax)) {
                    EntryBox = SNew(SNumericEntryBox<float>)
                        .Value_Lambda(FnGetValue)
                        .OnValueChanged_Lambda(FnSetValue);
                }
                else {
                    EntryBox = SNew(SNumericEntryBox<float>)
                        .AllowSpin(true)
                        .MinSliderValue(SliderMin)
                        .MaxSliderValue(SliderMax)
                        .Value_Lambda(FnGetValue)
                        .OnValueChanged_Lambda(FnSetValue);
                }

                ////////////// Reset to default handlers //////////////
                TFunction<bool()> FnIsResetToDefaultVisible = [MaterialInstance, ParamInfo]() {
                    float DefaultValue{}, CurrentValue{};
                    MaterialInstance->GetScalarParameterDefaultValue(ParamInfo, DefaultValue);
                    MaterialInstance->GetScalarParameterValue(ParamInfo, CurrentValue);
                    return !FMath::IsNearlyEqual(DefaultValue, CurrentValue);
                };

                TFunction<void()> FnHandleResetToDefaultClicked = [MaterialInstance, MarkDirty, ParamInfo]() {
                    float DefaultValue{};
                    MaterialInstance->GetScalarParameterDefaultValue(ParamInfo, DefaultValue);
                    MaterialInstance->SetScalarParameterValueEditorOnly(ParamInfo, DefaultValue);
                    
                    UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);
                    MarkDirty();
                };
                ///////////////////////////////////////////////////////

                FPropertyEntry& PropertyEntry = PropertyEntries.AddDefaulted_GetRef();
                MaterialInstance->GetParameterSortPriority(ParamInfo, PropertyEntry.SortPriority);
                MaterialInstance->GetGroupName(ParamInfo, PropertyEntry.Category);
                PropertyEntry.ParamName = ParamInfo.Name;
                PropertyEntry.Content = SNew(SHorizontalBox)
                    +SHorizontalBox::Slot()
                    .FillWidth(1.0)
                    [
                        EntryBox.ToSharedRef()
                    ]
                    +SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        MakeResetToDefaultWidget(FnIsResetToDefaultVisible, FnHandleResetToDefaultClicked)
                    ];
                
            }
        }
    }

    // Vector Color Parameters
    {
        TArray<FMaterialParameterInfo> VectorMaterialParams;
        MaterialInstance->GetAllVectorParameterInfo(VectorMaterialParams, TempGuids);
        for (const FMaterialParameterInfo& ParamInfo : VectorMaterialParams) {
            if (ParamInfo.Association == LayerParameter && ParamInfo.Index == CanvasMatLayer->LayerIndex) {
                TSharedPtr<SBox> ParentBlockParent = SNew(SBox);
                TSharedPtr<SColorBlock> ColorBlockWidget = SNew(SColorBlock)
                    .AlphaBackgroundBrush(FAppStyle::Get().GetBrush("ColorPicker.RoundedAlphaBackground"))
                    .IsEnabled(true)
                    .ShowBackgroundForAlpha(true)
                    .AlphaDisplayMode(EColorBlockAlphaDisplayMode::Separate)
                    .Size(FVector2D(70.0f, 20.0f))
                    .CornerRadius(FVector4(4.0f,4.0f,4.0f,4.0f))
                    .Color_Lambda([MaterialInstanceWeakPtr, ParamInfo]() {
                        if (const UMaterialInstanceConstant* Mat = MaterialInstanceWeakPtr.Get()) {
                            FLinearColor Value{};
                            Mat->GetVectorParameterValue(ParamInfo, Value);
                            return Value;
                        }
                        else {
                            return FLinearColor::Black;
                        }
                    })
                    .OnMouseButtonDown_Lambda([MaterialInstanceWeakPtr, ParamInfo, ParentBlockParent, CanvasBP, CanvasMatLayer](const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) {
                        FLinearColor InitialColor = FLinearColor::Black;
                        if (const UMaterialInstanceConstant* Mat = MaterialInstanceWeakPtr.Get()) {
                            Mat->GetVectorParameterValue(ParamInfo, InitialColor);
                        }

                        FDungeonCanvasColorPicker::FSettings PickerSettings;
                        PickerSettings.InitialColor = InitialColor;
                        PickerSettings.ParamInfo = ParamInfo;
                        PickerSettings.MaterialInstanceWeakPtr = MaterialInstanceWeakPtr;
                        PickerSettings.ParentDetailsWidget = ParentBlockParent;
                        PickerSettings.CanvasBlueprintPtr = CanvasBP;
                        PickerSettings.CanvasMatLayerPtr = CanvasMatLayer;

                        FDungeonCanvasColorPicker::Get().ShowColorPicker(PickerSettings);
                        return FReply::Handled();
                    });
                
                ParentBlockParent->SetContent(ColorBlockWidget.ToSharedRef());

                
                ////////////// Reset to default handlers //////////////
                TFunction<bool()> FnIsResetToDefaultVisible = [MaterialInstance, ParamInfo]() {
                    FLinearColor DefaultValue{}, CurrentValue{};
                    MaterialInstance->GetVectorParameterDefaultValue(ParamInfo, DefaultValue);
                    MaterialInstance->GetVectorParameterValue(ParamInfo, CurrentValue);
                    return DefaultValue != CurrentValue;
                };

                TFunction<void()> FnHandleResetToDefaultClicked = [MaterialInstance, ParamInfo, MarkDirty]() {
                    FLinearColor DefaultValue{};
                    MaterialInstance->GetVectorParameterDefaultValue(ParamInfo, DefaultValue);
                    MaterialInstance->SetVectorParameterValueEditorOnly(ParamInfo, DefaultValue);
                    
                    UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);
                    MarkDirty();
                };
                ///////////////////////////////////////////////////////

                FPropertyEntry& PropertyEntry = PropertyEntries.AddDefaulted_GetRef();
                MaterialInstance->GetParameterSortPriority(ParamInfo, PropertyEntry.SortPriority);
                MaterialInstance->GetGroupName(ParamInfo, PropertyEntry.Category);
                PropertyEntry.ParamName = ParamInfo.Name;
                PropertyEntry.Content = SNew(SHorizontalBox)
                    +SHorizontalBox::Slot()
                    .FillWidth(1.0)
                    [
                        ParentBlockParent.ToSharedRef()
                    ]
                    +SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        MakeResetToDefaultWidget(FnIsResetToDefaultVisible, FnHandleResetToDefaultClicked)
                    ];
            }
        }
    }

    // Texture Parameter
    {
        TArray<FMaterialParameterInfo> TextureMaterialParams;
        MaterialInstance->GetAllTextureParameterInfo(TextureMaterialParams, TempGuids);
        for (const FMaterialParameterInfo& ParamInfo : TextureMaterialParams) {
            if (ParamInfo.Association == LayerParameter && ParamInfo.Index == CanvasMatLayer->LayerIndex) {
                
                ////////////// Reset to default handlers //////////////
                TFunction<bool()> FnIsResetToDefaultVisible = [MaterialInstance, ParamInfo]() {
                    UTexture* DefaultValue{};
                    UTexture* CurrentValue{};
                    MaterialInstance->GetTextureParameterDefaultValue(ParamInfo, DefaultValue);
                    MaterialInstance->GetTextureParameterValue(ParamInfo, CurrentValue);
                    return DefaultValue != CurrentValue;
                };

                TFunction<void()> FnHandleResetToDefaultClicked = [MaterialInstance, ParamInfo, MarkDirty]() {
                    UTexture* DefaultValue{};
                    MaterialInstance->GetTextureParameterDefaultValue(ParamInfo, DefaultValue);
                    MaterialInstance->SetTextureParameterValueEditorOnly(ParamInfo, DefaultValue);
                    
                    UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);
                    MarkDirty();
                };
                ///////////////////////////////////////////////////////

                FPropertyEntry& PropertyEntry = PropertyEntries.AddDefaulted_GetRef();
                MaterialInstance->GetParameterSortPriority(ParamInfo, PropertyEntry.SortPriority);
                MaterialInstance->GetGroupName(ParamInfo, PropertyEntry.Category);
                PropertyEntry.ParamName = ParamInfo.Name;
                PropertyEntry.Content = SNew(SHorizontalBox)
                    +SHorizontalBox::Slot()
                    .FillWidth(1.0)
                    [
                        SNew(SObjectPropertyEntryBox)
                        .AllowedClass(UTexture::StaticClass())
                        .AllowClear(true)
                        .DisplayUseSelected(true)
                        .DisplayBrowse(true)
                        .DisplayThumbnail(true)
			            .ThumbnailPool(DetailBuilder.GetThumbnailPool())
                        .ObjectPath_Lambda([ParamInfo, MaterialInstanceWeakPtr]() {
                            UTexture* Texture{};
                            if (const UMaterialInstanceConstant* Mat = MaterialInstanceWeakPtr.Get()) {
                                Mat->GetTextureParameterValue(ParamInfo, Texture);
                            }
                            return Texture ? Texture->GetPathName() : "";
                        })
                        .OnObjectChanged_Lambda([ParamInfo, MaterialInstanceWeakPtr, MarkDirty](const FAssetData& InAssetData) {
                            if (UMaterialInstanceConstant* Mat = MaterialInstanceWeakPtr.Get()) {
                                UTexture* NewTexture = Cast<UTexture>(InAssetData.GetAsset());
                                Mat->SetTextureParameterValueEditorOnly(ParamInfo, NewTexture);
                                UMaterialEditingLibrary::UpdateMaterialInstance(Mat);
                                MarkDirty();
                            }
                        })
                    ]
                    +SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        MakeResetToDefaultWidget(FnIsResetToDefaultVisible, FnHandleResetToDefaultClicked)
                    ];
            }
        }
    }

    PropertyEntries.Sort([](const FPropertyEntry& A, const FPropertyEntry& B) {
        return A.SortPriority < B.SortPriority;
    });

    TMap<FName, IDetailCategoryBuilder*> CategoriesByName;
    const FName InternalCategoryName = "Internal";
    const FName DefaultCategoryName = "Material Layer Inputs";
    IDetailCategoryBuilder& DefaultFallbackCategory = DetailBuilder.EditCategory("Material Layer Inputs", FText::GetEmpty(), ECategoryPriority::Uncommon);
    for (const FPropertyEntry& Entry : PropertyEntries) {
        FName CategoryName = Entry.Category.IsNone() ? DefaultCategoryName : Entry.Category;
        if (CategoryName == InternalCategoryName) {
            // Do no display internal parameters
            continue;
        }
        IDetailCategoryBuilder* Category{};
        if (IDetailCategoryBuilder** SearchResult = CategoriesByName.Find(CategoryName)) {
            Category = *SearchResult;
        }
        else {
            FString UnparsedCategoryName = CategoryName.ToString();
            if (UnparsedCategoryName.IsEmpty()) {
                Category = &DefaultFallbackCategory;
            }
            else {
                FString DisplayCategoryName = UnparsedCategoryName;
                int32 SortOrder = INDEX_NONE;
            
                FRegexPattern Pattern(TEXT("^(.+?)\\s*(?:\\[(\\d+)\\])?$"));
                FRegexMatcher Matcher(Pattern, DisplayCategoryName);
                if (Matcher.FindNext()) {
                    DisplayCategoryName = UnparsedCategoryName.Mid(Matcher.GetCaptureGroupBeginning(1), Matcher.GetCaptureGroupEnding(1) - Matcher.GetCaptureGroupBeginning(1));
                    if (Matcher.GetCaptureGroupBeginning(2) != INDEX_NONE) {
                        FString SortOrderString = UnparsedCategoryName.Mid(Matcher.GetCaptureGroupBeginning(2), Matcher.GetCaptureGroupEnding(2) - Matcher.GetCaptureGroupBeginning(2));
                        SortOrder = FCString::Atoi(*SortOrderString);
                    }
                }
            
                Category = &DetailBuilder.EditCategory(FName(DisplayCategoryName), FText::GetEmpty(), ECategoryPriority::Uncommon);
                Category->SetSortOrder(5000 + SortOrder);
                CategoriesByName.Add(Entry.Category, Category);
            }
        }
        if (!Category) {
            continue;
        }

        FString DisplayString = FName::NameToDisplayString(Entry.ParamName.ToString(), false);
        Category->AddCustomRow(FText::FromName(Entry.ParamName))
            .NameContent()
            [
                SNew(STextBlock).Text(FText::FromString(DisplayString))
            ]
            .ValueContent()
            .MinDesiredWidth(125.f)
            [
                Entry.Content.ToSharedRef()
            ];
    }

    IDetailCategoryBuilder& MaterialFunctionsCategory = DetailBuilder.EditCategory("Material Functions", FText::GetEmpty(), ECategoryPriority::Uncommon);
    MaterialFunctionsCategory.SetSortOrder(6000);

}

TSharedRef<IDetailCustomization> FDungeonCanvasMaterialLayerCustomization::MakeInstance() {
    return MakeShareable(new FDungeonCanvasMaterialLayerCustomization);
}


#undef LOCTEXT_NAMESPACE

