#include "CFLSolver/CFLBase.h"
#include "CFLData/EdgeDump.h"
#include <fstream>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>

using namespace SVF;

// === TYPES & BASIC VARIABLES ===

enum NodeFlags {
    None   = 0x0,
    Entry  = 0x1,
    Exit   = 0x2,
    Call   = 0x4,
    Return = 0x8,
    Branch = 0x10
};

inline NodeFlags operator&(NodeFlags a, NodeFlags b) {
    return (NodeFlags)((unsigned)(a) & (unsigned)(b));
}

inline NodeFlags operator|(NodeFlags a, NodeFlags b) {
    return (NodeFlags)((unsigned)(a) | (unsigned)(b));
}

inline NodeFlags& operator|=(NodeFlags& a, NodeFlags b) {
    a = a | b;
    return a;
}

typedef std::pair<std::pair<NodeID, NodeID>, Label> EdgeDesc;

static bool initialized = false;

// node type: entry, normal, exit or entry-exit
static std::unordered_map<NodeID, NodeFlags> nodeFlagsMap;
static std::vector<EdgeDesc> edges;
static std::unordered_map<CFGSymbTy, std::string> intToSymbMap;
// nodes that have an a/call_i/ret_i edge going out of them
// this is to determine branching and conditional calls/returns
static std::unordered_set<NodeID> srcNodes;

char const *const callSymName = "call_i";
char const *const retSymName = "ret_i";
char const *const assignSymName = "a";
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

    nodeFlagsMap = std::unordered_map<NodeID, NodeFlags>();
    edges = std::vector<EdgeDesc>();
    dsuNodePrev = std::unordered_map<NodeID, NodeID>();
    dsuNodeRank = std::unordered_map<NodeID, unsigned>();
    intToSymbMap = std::unordered_map<CFGSymbTy, std::string>();
    srcNodes = std::unordered_set<NodeID>();
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

    if (intToSymbMap.find(ty.first) == intToSymbMap.end())
        return;

    if (nodeFlagsMap.find(src) == nodeFlagsMap.end())
        nodeFlagsMap[src] = None;
    if (nodeFlagsMap.find(dest) == nodeFlagsMap.end())
        nodeFlagsMap[dest] = None;
    
    if (intToSymbMap[ty.first] == callSymName ||
        intToSymbMap[ty.first] == retSymName  ||
        intToSymbMap[ty.first] == assignSymName) {
        if (srcNodes.find(src) == srcNodes.end())
            srcNodes.insert(src);
        else if (!(((nodeFlagsMap[src] & Exit) != 0) && (intToSymbMap[ty.first] == retSymName))) {
            // two or more a/call_i/ret_i edges out of one node
            // multiple returns do not count
            nodeFlagsMap[src] |= Branch;
        }
    }

    if (intToSymbMap[ty.first] == callSymName) {
        nodeFlagsMap[src]  |= Call;
        nodeFlagsMap[dest] |= Entry;
    }

    if (intToSymbMap[ty.first] == retSymName) {
        nodeFlagsMap[src]  |= Exit;
        nodeFlagsMap[dest] |= Return;
    }

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
        nodesFile<<"NodeID,NodeFlags,FunctionID\n";

        for (auto &nodeDesc: nodeFlagsMap) {
            NodeFlags flags = nodeDesc.second;
            if (flags == None)
                continue;

            nodesFile<<nodeDesc.first<<",";

            std::vector<std::string> flagNames;
            if (flags & Entry)
                flagNames.push_back("Entry");
            if (flags & Exit)
                flagNames.push_back("Exit");
            if (flags & Call)
                flagNames.push_back("Call");
            if (flags & Return)
                flagNames.push_back("Return");
            if (flags & Branch)
                flagNames.push_back("Branch");

            for (auto fn: flagNames) {
                nodesFile<<fn;
                if (fn != flagNames[flagNames.size() - 1])
                    nodesFile<<"|";
            }
            nodesFile<<",";
            nodesFile<<functionIDs[dsuRoot(nodeDesc.first)]<<"\n";
        }

        nodesFile.close();
    }

    // ~~~ edges dump ~~~

    std::ofstream edgesFile;
    edgesFile.open(fileName);
    edgesFile<<"Src,Dest,Nterm\n";

    for (auto &edge: edges) {
        Label ty = edge.second;
        if (intToSymbMap.find(ty.first) == intToSymbMap.end())
            continue;
        
        NodeFlags srcFlags = nodeFlagsMap[edge.first.first];
        NodeFlags destFlags = nodeFlagsMap[edge.first.second];
        if (srcFlags == None || destFlags == None)
            continue;

        std::string symb = intToSymbMap[ty.first];
        
        edgesFile<<edge.first.first<<","<<edge.first.second<<",";
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