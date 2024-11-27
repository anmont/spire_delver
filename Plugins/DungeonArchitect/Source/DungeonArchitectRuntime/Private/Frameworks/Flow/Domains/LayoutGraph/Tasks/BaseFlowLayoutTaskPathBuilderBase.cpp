//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Flow/Domains/LayoutGraph/Tasks/BaseFlowLayoutTaskPathBuilderBase.h"

#include "Frameworks/Flow/Domains/FlowDomain.h"
#include "Frameworks/Flow/Domains/LayoutGraph/Core/FlowAbstractGraphConstraints.h"
#include "Frameworks/Flow/Domains/LayoutGraph/Tasks/Lib/FlowAbstractGraphPathUtils.h"

void UBaseFlowLayoutTaskPathBuilderBase::FinalizePath(const FFlowAGStaticGrowthState& StaticState, FFlowAGGrowthState& State) const {
    auto FnFinalizeNode = [this](UFlowAbstractNode* InMergedNode, const FFlowAGGrowthState_PathItem& PathItem) { FinalizePathNode(InMergedNode, PathItem); };
    FFlowAbstractGraphPathUtils::FinalizePath(StaticState, State, FnFinalizeNode);
}

IFlowAGNodeGroupGeneratorPtr UBaseFlowLayoutTaskPathBuilderBase::CreateNodeGroupGenerator(TWeakPtr<const IFlowDomain> InDomainPtr) const {
    return MakeShareable(new FNullFlowAGNodeGroupGenerator);
}

FFlowAbstractGraphConstraintsPtr UBaseFlowLayoutTaskPathBuilderBase::CreateGraphConstraints(TWeakPtr<const IFlowDomain> InDomainPtr) const {
    return MakeShareable(new FNullFlowAbstractGraphConstraints);
}

UFlowLayoutNodeCreationConstraint* UBaseFlowLayoutTaskPathBuilderBase::GetNodeCreationConstraintLogic() const {
    return nullptr;
}

