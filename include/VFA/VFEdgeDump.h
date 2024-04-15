#ifndef VFEDGEDUMP_H
#define VFEDGEDUMP_H

#include "CFLSolver/CFLBase.h"

void initEdgeDump();

void dumpEdge(SVF::NodeID src, SVF::NodeID dest, SVF::Label ty);

void saveEdgesToFile(std::string fileName, bool dumpNodes = false);

#endif // VFEDGEDUMP_H