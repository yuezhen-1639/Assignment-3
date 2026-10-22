/**
 * CFLR.cpp
 * @author kisslune 
 */

#include "A4Header.h"
#include <unordered_set>
#include <vector>

using namespace SVF;
using namespace llvm;
using namespace std;

int main(int argc, char **argv)
{
    auto moduleNameVec =
            OptionBase::parseOptions(argc, argv, "Whole Program Points-to Analysis",
                                     "[options] <input-bitcode...>");

    LLVMModuleSet::buildSVFModule(moduleNameVec);

    SVFIRBuilder builder;
    auto pag = builder.build();
    pag->dump();

    CFLR solver;
    solver.buildGraph(pag);
    // TODO: complete this method
    solver.solve();
    solver.dumpResult();

    LLVMModuleSet::releaseLLVMModuleSet();
    return 0;
}


void CFLR::solve()
{
    // TODO: complete this function. The implementations of graph and worklist are provided.
    //  You need to:
    //  1. implement the grammar production rules into code;
    //  2. implement the dynamic-programming CFL-reachability algorithm.
    //  You may need to add your new methods to 'CFLRGraph' and 'CFLR'.

    if (!graph)
        return;

    struct Production
    {
        EdgeLabel lhs;
        EdgeLabel first;
        EdgeLabel second;
    };

    vector<Production> productions = {
            {PT, VFBar, AddrBar},
            {VF, VF, VF},
            {VF, SV, Load},
            {VF, PV, Load},
            {VF, Store, VP},
            {VA, LV, Load},
            {VA, VFBar, VA},
            {VA, VA, VF},
            {SV, Store, VA},
            {PV, PTBar, VA},
            {VP, VA, PT},
            {LV, LoadBar, VA}
    };

    const auto productionCount = productions.size();
    for (size_t i = 0; i < productionCount; ++i)
    {
        const auto rule = productions[i];
        productions.push_back(
                {rule.lhs ^ 1u, rule.second ^ 1u, rule.first ^ 1u});
    }

    workList.clear();
    unordered_set<unsigned> nodes;
    for (const auto &node : graph->getSuccessorMap())
    {
        nodes.insert(node.first);
        for (const auto &label : node.second)
            for (auto dst : label.second)
            {
                nodes.insert(dst);
                workList.push(CFLREdge(node.first, dst, label.first));
            }
    }

    auto addEdge = [&](unsigned src, unsigned dst, EdgeLabel label)
    {
        if (!graph->hasEdge(src, dst, label))
        {
            graph->addEdge(src, dst, label);
            workList.push(CFLREdge(src, dst, label));
        }
    };

    for (auto node : nodes)
        for (auto label : {VF, VFBar, VA, VABar})
            addEdge(node, node, label);

    const auto &successors = graph->getSuccessorMap();
    const auto &predecessors = graph->getPredecessorMap();

    while (!workList.empty())
    {
        const auto edge = workList.pop();
        vector<CFLREdge> inferred;

        if (edge.label == Copy)
            inferred.emplace_back(edge.src, edge.dst, VF);
        else if (edge.label == CopyBar)
            inferred.emplace_back(edge.src, edge.dst, VFBar);

        for (const auto &rule : productions)
        {
            if (edge.label == rule.first)
            {
                const auto node = successors.find(edge.dst);
                if (node != successors.end())
                {
                    const auto label = node->second.find(rule.second);
                    if (label != node->second.end())
                        for (auto dst : label->second)
                            inferred.emplace_back(edge.src, dst, rule.lhs);
                }
            }

            if (edge.label == rule.second)
            {
                const auto node = predecessors.find(edge.src);
                if (node != predecessors.end())
                {
                    const auto label = node->second.find(rule.first);
                    if (label != node->second.end())
                        for (auto src : label->second)
                            inferred.emplace_back(src, edge.dst, rule.lhs);
                }
            }
        }

        for (const auto &edgeToAdd : inferred)
            addEdge(edgeToAdd.src, edgeToAdd.dst, edgeToAdd.label);
    }
}
