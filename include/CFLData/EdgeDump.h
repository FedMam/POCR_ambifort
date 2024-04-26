#ifndef EDGEDUMP_H
#define EDGEDUMP_H

#include "CFLSolver/CFLBase.h"

void initEdgeDump();
void edgeDumpSetSymbol(SVF::CFGSymbTy sym, std::string symName);
void dumpEdge(SVF::NodeID src, SVF::NodeID dest, SVF::Label ty);
void saveEdgesToFile(std::string fileName, bool dumpNodes = false, bool dumpCallGraph = false);

#endif // EDGEDUMP_H