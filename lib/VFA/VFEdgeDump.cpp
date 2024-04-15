#include "CFLSolver/CFLBase.h"
#include "VFA/VFEdgeDump.h"
#include "VFA/VFAnalysis.h"
#include <fstream>
#include <vector>
#include <unordered_map>

using namespace SVF;

enum NodeClass {
    Normal,
    Entry,
    Exit,
    EntryExit
};

typedef std::pair<std::pair<NodeID, NodeID>, Label> EdgeDesc;

static bool initialized = false;

// node type: entry, normal, exit or entry-exit
static std::unordered_map<NodeID, NodeClass> nodeMap;
static std::vector<EdgeDesc> edges;

static unsigned long numFunctions = 0;

void initEdgeDump() {
    initialized = true;

    nodeMap = std::unordered_map<NodeID, NodeClass>();
    edges = std::vector<EdgeDesc>();
}

void dumpEdge(NodeID src, NodeID dest, Label ty) {
    if (!initialized) return;

    edges.push_back({{src, dest}, ty});

    if (ty.first == VFAnalysis::call) {
        if (nodeMap.find(dest) != nodeMap.end()) {
            NodeClass nodeClass = nodeMap[dest];
            if (nodeClass == Normal)
                nodeMap[dest] = Entry;
            else if (nodeClass == Exit)
                nodeMap[dest] = EntryExit;
        }
        else nodeMap[dest] = Entry;
    } 
    else if (nodeMap.find(dest) == nodeMap.end())
        nodeMap[dest] = Normal;

    if (ty.first == VFAnalysis::ret) {
        if (nodeMap.find(src) != nodeMap.end()) {
            NodeClass nodeClass = nodeMap[src];
            if (nodeClass == Normal)
                nodeMap[src] = Exit;
            else if (nodeClass == Entry)
                nodeMap[src] = EntryExit;
        } else nodeMap[src] = Exit;
    }
    else if (nodeMap.find(src) == nodeMap.end())
        nodeMap[src] = Normal;
}

void saveEdgesToFile(std::string fileName, bool dumpNodes) {
    if (!initialized) return;

    if (dumpNodes) {
        std::ofstream nodesFile;
        nodesFile.open(fileName + ".nodes");
        nodesFile<<"NodeID,NodeClass\n";

        for (auto &nodeDesc: nodeMap) {
            nodesFile<<nodeDesc.first<<",";
            switch(nodeDesc.second) {
                case Normal:
                    nodesFile<<"Normal\n"; break;
                case Entry:
                    nodesFile<<"Entry\n"; break;
                case Exit:
                    nodesFile<<"Exit\n"; break;
                case EntryExit:
                    nodesFile<<"EntryExit\n"; break;
            }
        }

        nodesFile.close();
    }

    std::ofstream edgesFile;
    edgesFile.open(fileName);
    edgesFile<<"Src,Dest,Nterm\n";

    for (auto &edge: edges) {
        edgesFile<<edge.first.first<<","<<edge.first.second<<",";
        Label ty = edge.second;
        switch (ty.first) {
            case VFAnalysis::a:
                edgesFile<<"a\n"; break;
            case VFAnalysis::A:
                edgesFile<<"A\n"; break;
            case VFAnalysis::call:
                edgesFile<<"call_"<<ty.second<<"\n"; break;
            case VFAnalysis::ret:
                edgesFile<<"ret_"<<ty.second<<"\n"; break;
            case VFAnalysis::Cl:
                edgesFile<<"Cl_"<<ty.second<<"\n"; break;
            case VFAnalysis::B:
                edgesFile<<"B\n"; break;
            case VFAnalysis::fault:
                edgesFile<<"fault\n"; break;
            default:
                edgesFile<<"unknown\n"; break;
        }
    }

    edgesFile.close();
}