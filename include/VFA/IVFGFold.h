/* -------------------- IVFGFold.h ------------------ */
//
// Created by kisslune on 2/23/23.
//

#ifndef POCR_SVF_IVFGFOLD_H
#define POCR_SVF_IVFGFOLD_H

#include "CFLData/IVFG.h"

namespace SVF
{
/*!
 * Graph folding instance for VFGs
 */
class IVFGFold
{
private:
    IVFG* lg;
    std::stack<NodePair> foldablePairs;

public:
    IVFGFold(IVFG* g) : lg(g)
    {}

    void foldGraph();
};

}


#endif //POCR_SVF_IVFGFOLD_H
