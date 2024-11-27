//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Flow/Domains/LayoutGraph/Impl/GridFlow/GridFlowLayoutEdGraph.h"

#include "Frameworks/FlowImpl/GridFlow/LayoutGraph/GridFlowAbstractGraph.h"

UGridFlowAbstractGraph* UGridFlowLayoutEdGraph::GetGridScriptGraph() const {
	return Cast<UGridFlowAbstractGraph>(ScriptGraph);
}

