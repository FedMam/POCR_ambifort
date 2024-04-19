#include "CFLSolver/CFLBase.h"
#include "VFA/VFEdgeDump.h"
#include "VFA/VFAnalysis.h"
#include <fstream>
#include <vector>
#include <set>
#include <unordered_map>

using namespace SVF;

// === TYPES & BASIC VARIABLES ===

enum NodeType {
    Normal,
    Entry,
    Exit,
    EntryExit
};

typedef std::pair<std::pair<NodeID, NodeID>, Label> EdgeDesc;

static bool initialized = false;

// node type: entry, normal, exit or entry-exit
static std::unordered_map<NodeID, NodeType> nodeTypeMap;
static std::vector<EdgeDesc> edges;

// === DISJOINT SET UNION ===

static std::unordered_map<NodeID, NodeID>   dsuNodePrev;
static std::unordered_map<NodeID, unsigned> dsuNodeRank; // ranks heuristic

static void dsuNewNode(NodeID node) {
    // initially, each node belongs to its own function
    dsuNodePrev[node] = node;
    dsuNodeRank[node] = 1;
}

static void dsuNewNodeIfNotExists(NodeID node) {
    if (dsuNodePrev.find(node) == dsuNodePrev.end())
        dsuNewNode(node);   
}

static NodeID dsuRoot(NodeID node) {
    dsuNewNodeIfNotExists(node);

    NodeID root = node;
    while (dsuNodePrev[root] != root)
        root = dsuNodePrev[root];
    
    dsuNodePrev[node] = root; // path compression heuristic
    return root;
}

static void dsuUnite(NodeID nodeA, NodeID nodeB) {
    NodeID rootA = dsuRoot(nodeA), rootB = dsuRoot(nodeB);
    if (rootA == rootB)
        return;

    unsigned rankA = dsuNodeRank[rootA], rankB = dsuNodeRank[rootB];
    if (rankA > rankB)
        dsuNodePrev[rootB] = rootA;
    else if (rankB > rankA)
        dsuNodePrev[rootA] = rootB;
    else {
        dsuNodePrev[rootB] = rootA;
        dsuNodeRank[rootA]++;
    }
}

// === DUMP FUNCTIONS ===

void initEdgeDump() {
    initialized = true;

    nodeTypeMap = std::unordered_map<NodeID, NodeType>();
    edges = std::vector<EdgeDesc>();
    dsuNodePrev = std::unordered_map<NodeID, NodeID>();
    dsuNodeRank = std::unordered_map<NodeID, unsigned>();
}

void dumpEdge(NodeID src, NodeID dest, Label ty) {
    if (!initialized) return;

    edges.push_back({{src, dest}, ty});

    dsuNewNodeIfNotExists(src);
    dsuNewNodeIfNotExists(dest);

    if (ty.first == VFAnalysis::call) {
        if (nodeTypeMap.find(dest) != nodeTypeMap.end()) {
            NodeType nodeType = nodeTypeMap[dest];
            if (nodeType == Normal)
                nodeTypeMap[dest] = Entry;
            else if (nodeType == Exit)
                nodeTypeMap[dest] = EntryExit;
        }
        else nodeTypeMap[dest] = Entry;
    } 
    else if (nodeTypeMap.find(dest) == nodeTypeMap.end())
        nodeTypeMap[dest] = Normal;

    if (ty.first == VFAnalysis::ret) {
        if (nodeTypeMap.find(src) != nodeTypeMap.end()) {
            NodeType nodeType = nodeTypeMap[src];
            if (nodeType == Normal)
                nodeTypeMap[src] = Exit;
            else if (nodeType == Entry)
                nodeTypeMap[src] = EntryExit;
        } else nodeTypeMap[src] = Exit;
    }
    else if (nodeTypeMap.find(src) == nodeTypeMap.end())
        nodeTypeMap[src] = Normal;

    if (ty.first == VFAnalysis::A) {
        // two nodes belong to one function if and only
        // if they are connected with an 'A' edge
        dsuUnite(src, dest);
    }
}

void saveEdgesToFile(std::string fileName, bool dumpNodes) {
    if (!initialized) return;

    // ~~~ nodes dump ~~~

    if (dumpNodes) {
        // here, we map the root node IDs from the DSU
        // to values starting from 1. These values will
        // serve as function IDs in the resulting file.
        // So, instead of f12, f37, f844, f6172 we will
        // get f1, f2, f3, f4.
        std::set<NodeID> dsuRootIDs;
        for (auto nodePrev: dsuNodePrev) {
            NodeID node = nodePrev.first;
            NodeID root = dsuRoot(node);
            dsuRootIDs.insert(root);
        }
        std::unordered_map<NodeID, unsigned> functionIDs;
        unsigned rootIDCurr = 1;
        for (auto root: dsuRootIDs) {
            functionIDs[root] = rootIDCurr;
            rootIDCurr++;
        }

        std::ofstream nodesFile;
        nodesFile.open(fileName + ".nodes");
        nodesFile<<"NodeID,NodeType,FunctionID\n";

        for (auto &nodeDesc: nodeTypeMap) {
            nodesFile<<nodeDesc.first<<",";
            switch(nodeDesc.second) {
                case Normal:
                    nodesFile<<"Normal,"; break;
                case Entry:
                    nodesFile<<"Entry,"; break;
                case Exit:
                    nodesFile<<"Exit,"; break;
                case EntryExit:
                    nodesFile<<"EntryExit,"; break;
            }
            nodesFile<<functionIDs[dsuRoot(nodeDesc.first)]<<"\n";
        }

        nodesFile.close();
    }

    // ~~~ edges dump ~~~

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