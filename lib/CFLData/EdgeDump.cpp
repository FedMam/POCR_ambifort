#include "CFLSolver/CFLBase.h"
#include "CFLData/EdgeDump.h"
#include <fstream>
#include <vector>
#include <set>
#include <unordered_set>
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
static std::unordered_map<CFGSymbTy, std::string> intToSymbMap;

char const *const callSymName = "call_i";
char const *const retSymName = "ret_i";
char const *const intraFuncSymName = "A";

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
    intToSymbMap = std::unordered_map<CFGSymbTy, std::string>();
}

void edgeDumpSetSymbol(CFGSymbTy sym, std::string symName) {
    if (!initialized) return;

    intToSymbMap[sym] = symName;
}

void dumpEdge(NodeID src, NodeID dest, Label ty) {
    if (!initialized) return;

    edges.push_back({{src, dest}, ty});

    dsuNewNodeIfNotExists(src);
    dsuNewNodeIfNotExists(dest);

    if (intToSymbMap[ty.first] == callSymName) {
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

    if (intToSymbMap[ty.first] == retSymName) {
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

    if (intToSymbMap[ty.first] == intraFuncSymName) {
        // two nodes belong to one function if and only
        // if they are connected with an 'A' edge
        dsuUnite(src, dest);
    }
}

void saveEdgesToFile(std::string fileName, bool dumpNodes, bool dumpCallGraph) {
    if (!initialized) return;

    std::unordered_map<NodeID, unsigned> functionIDs;
    if (dumpNodes || dumpCallGraph) {
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
        functionIDs = std::unordered_map<NodeID, unsigned>();
        unsigned rootIDCurr = 1;
        for (auto root: dsuRootIDs) {
            functionIDs[root] = rootIDCurr;
            rootIDCurr++;
        }
    }

    // ~~~ nodes dump ~~~
    
    if (dumpNodes) {
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
        std::string symb = intToSymbMap[ty.first];
        if (symb[symb.length() - 2] == '_' && symb[symb.length() - 1] == 'i') {
            edgesFile<<symb.substr(0, symb.length() - 1); // without i
            edgesFile<<ty.second<<"\n";
        }
        else edgesFile<<symb<<"\n";
    }
    edgesFile.close();

    // ~~~ callgraph dump ~~~

    if (dumpCallGraph) {
        std::unordered_set<std::pair<unsigned, unsigned>> callGraphEdges;
        for (auto &edge: edges) {
            Label ty = edge.second;
            unsigned srcFuncID = functionIDs[dsuRoot(edge.first.first )];
            unsigned dstFuncID = functionIDs[dsuRoot(edge.first.second)];
            if (intToSymbMap[ty.first] == callSymName) {
                callGraphEdges.insert({srcFuncID, dstFuncID});
            }
        }

        std::ofstream callGraphFile;
        callGraphFile.open(fileName + ".callgraph");
        callGraphFile<<"Src,Dest\n";

        for (auto &cgedge: callGraphEdges) {
            callGraphFile<<"f"<<cgedge.first<<",f"<<cgedge.second<<"\n";
        }

        callGraphFile.close();
    }
}