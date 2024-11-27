//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/MarkerGenerator/MarkerGenProcessor.h"

#include "Frameworks/MarkerGenerator/PatternScript/Impl/PatternScriptNodesImpl.h"
#include "Frameworks/MarkerGenerator/PatternScript/PatternScript.h"

namespace UMGScriptNodeLib {
	FString ApplyVariable(const FString& InText, const TMap<FString, FString>& InVariables) {
		if (InText.Len() > 2) {
			if (InText[0] == '{' && InText[InText.Len() - 1] == '}') {
				const FString VariableName = InText.Mid(1, InText.Len() - 2);
				if (const FString* ValuePtr = InVariables.Find(VariableName)) {
					return *ValuePtr;
				}
			}
		}
		return InText;
	}
}
///////////////////////////////// Action Nodes ///////////////////////////////// 
void UMGPatternScriptNode_OnPass::Execute(const FMGActionExecContext& InContext) {
	UMGPatternActionScriptNodeBase* NextNode = FMGPatternScriptUtils::GetOutgoingExecNode(this);
	TSet<UMGPatternActionScriptNodeBase*> Visited;
	while (NextNode && !Visited.Contains(NextNode)) {
		NextNode->Execute(InContext);
		Visited.Add(NextNode);
		
		NextNode = FMGPatternScriptUtils::GetOutgoingExecNode(NextNode);
	}
}

FString UMGPatternScriptNode_OnPass::GetActionText() const {
	return "";
}

void UMGPatternScriptNode_EmitMarker::Execute(const FMGActionExecContext& InContext) {
	if (InContext.Executor.IsValid()) {
		FMGExecEmitMarkerSettings EmitSettings;
		EmitSettings.CopyRotationFromMarkers = CopyRotationFromMarkers;
		EmitSettings.CopyHeightFromMarkers = CopyHeightFromMarkers;
		EmitSettings.DuplicateMarkerRemoveRule = DuplicateMarkerRemoveRule;
		const FString SubstitutedText = UMGScriptNodeLib::ApplyVariable(MarkerName, InContext.Variables);
		InContext.Executor->EmitMarker(SubstitutedText, EmitSettings);
	}
}

FString UMGPatternScriptNode_EmitMarker::GetActionText() const {
	const FString FriendlyMarkerName = (MarkerName.Len() == 0 ? TEXT("<NONE>") : *MarkerName);
	return FString::Printf(TEXT("ADD %s"), *FriendlyMarkerName);
}

void UMGPatternScriptNode_RemoveMarker::Execute(const FMGActionExecContext& InContext) {
	if (InContext.Executor.IsValid()) {
		const FString SubstitutedText = UMGScriptNodeLib::ApplyVariable(MarkerName, InContext.Variables);
		InContext.Executor->RemoveMarker(SubstitutedText);
	}
}

FString UMGPatternScriptNode_RemoveMarker::GetActionText() const {
	const FString FriendlyMarkerName = (MarkerName.Len() == 0 ? TEXT("<NONE>") : *MarkerName);
	return FString::Printf(TEXT("DEL %s"), *FriendlyMarkerName);
}

///////////////////////////////// Conditional Nodes /////////////////////////////////

#define PIN_TEXT(PinID) FMGPatternScriptUtils::GetPinRuleText(GetInputPin(PinID))

bool UMGPatternScriptNode_Result::Execute(const FMGConditionExecContext& InContext) {
	return GetInputPinValue(InContext, InPinID_Select);
}

FString UMGPatternScriptNode_Result::GenerateRuleText() const {
	const UMGPatternConditionalScriptNodeBase* IncomingNode = FMGPatternScriptUtils::GetIncomingConditionNode(this, InPinID_Select);
	if (!IncomingNode) return "";
	return FMGPatternScriptUtils::GetPinRuleText(GetInputPin(InPinID_Select), false);
}

bool UMGPatternScriptNode_LogicalAnd::Execute(const FMGConditionExecContext& InContext) {
	return GetInputPinValue(InContext, InPinID_A) && GetInputPinValue(InContext, InPinID_B);
}

FString UMGPatternScriptNode_LogicalAnd::GenerateRuleText() const {
	return PIN_TEXT(InPinID_A) + " AND " + PIN_TEXT(InPinID_B);
}

bool UMGPatternScriptNode_LogicalNot::Execute(const FMGConditionExecContext& InContext) {
	return !GetInputPinValue(InContext, InPinID_Default);
}

FString UMGPatternScriptNode_LogicalNot::GenerateRuleText() const {
	return "NOT " + PIN_TEXT(InPinID_Default);
}

bool UMGPatternScriptNode_LogicalOr::Execute(const FMGConditionExecContext& InContext) {
	return GetInputPinValue(InContext, InPinID_A) || GetInputPinValue(InContext, InPinID_B);
}

FString UMGPatternScriptNode_LogicalOr::GenerateRuleText() const {
	return PIN_TEXT(InPinID_A) + " OR " + PIN_TEXT(InPinID_B);
}

bool UMGPatternScriptNode_MarkerExists::Execute(const FMGConditionExecContext& InContext) {
	if (InContext.Executor.IsValid()) {
		const FString SubstitutedText = UMGScriptNodeLib::ApplyVariable(MarkerName, InContext.Variables);
		return InContext.Executor->ContainsMarker(SubstitutedText);
	}
	return false;
}

bool UMGPatternScriptNode_MarkerListExists::Execute(const FMGConditionExecContext& InContext) {
	if (InContext.Executor.IsValid()) {
		for (const FString& MarkerName : MarkerNames) {
			const FString SubstitutedText = UMGScriptNodeLib::ApplyVariable(MarkerName, InContext.Variables);
			if (InContext.Executor->ContainsMarker(SubstitutedText)) {
				return true;
			}
		} 
	}
	return false;
}


FString UMGPatternScriptNode_MarkerExists::GenerateRuleText() const {
	return FString::Printf(TEXT("%s"), MarkerName.Len() == 0 ? TEXT("<NONE>") : *MarkerName);
}

FString UMGPatternScriptNode_MarkerListExists::GenerateRuleText() const {
	if (MarkerNames.Num() == 0) {
		return TEXT("<NONE>");
	}
	FString Text;
	for (int i = 0; i < MarkerNames.Num(); i++) {
		if (i > 0) {
			Text += " OR ";
		}
		Text += MarkerNames[i];
	}
	if (MarkerNames.Num() > 1) {
		Text = "(" + Text + ")";
	}
	return Text;
}

